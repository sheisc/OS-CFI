#ifndef WPA_H_
#define WPA_H_

#include "DDA/DDAClient.h"
#include "MemoryModel/PointerAnalysis.h"
#include "Util/SCC.h"
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Pass.h>

#define DUMP_CFG_DEBUG 1

#define HASH_ID_RANGE 10000000

class SVFG;
class SVFGEdge;

/*!
 * Demand-Driven Pointer Analysis.
 * This class performs various pointer analysis on the given module.
 */

// [OS-CFI] we fix SUPA errored points-to for two reasons: 1) it points-to all
// address-taken 2) it points-to empty
typedef enum FIX_TYPE { OVER_APPROXIMATE = 1, UNDER_APPROXIMATE = 2 } fixType;
// [OC-CFI] we will have four different kinds of CFG
// 1) SUPA generated CI-CFG (Error included)
// 2) Origin Sensitive CFG (oCFG)
// 3) Callsite Sensitive CFG (cCFG)
// 4) Type and address taken CFG (Fix the error)
typedef enum CFG_TYPE { SUPA_CFG = 1, OCFG = 2, CCFG = 3, ATCFG = 3 } cfgType;

// [OS-CFI] oCFG Data Structure
// Origin Context can be null
typedef struct oCFGData {
  const llvm::Instruction *iCallInst;
  unsigned long iCallID;
  const llvm::Value *iCallTarget;
  unsigned long iCallTargetID;
  unsigned long originID;
  const llvm::Instruction *originCTXInst;
  unsigned long originCTXID;
} oCFG;

// [OS-CFI] cCFG Data Structure
// # of callsite will be variable
typedef struct cCFGData {
  const llvm::Instruction *iCallInst;
  unsigned long iCallID;
  const llvm::Value *iCallTarget;
  unsigned long iCallTargetID;
  std::vector<const llvm::Instruction *> *cInstStack;
  std::vector<unsigned long> *cIDStack;
} cCFG;

// [OS-CFI] Address Taken and Type Check CFG Data Structure
// the fixType is the enum defined above
typedef struct AddrTypeData {
  fixType type;
  const llvm::Instruction *iCallInst;
  unsigned long iCallID;
  const llvm::Value *iCallTarget;
  unsigned long iCallTargetID;
} atCFG;

// [OS-CFI] CI-CFG Data Structure
// Generated by SUPA
typedef struct SUPAData {
  const llvm::Instruction *iCallInst;
  unsigned long iCallID;
  const llvm::Value *iCallTarget;
  unsigned long iCallTargetID;
} supaCFG;

// [OS-CFI]Function* can map a set of Instruction*
typedef std::map<llvm::Function *, std::set<llvm::Instruction *>>
    FuncToInstSetMap;
typedef std::map<llvm::Function *, std::set<llvm::Instruction *>>::iterator
    FuncToInstSetMapIt;

// [OS-CFI] Function* can map a set of BasicBlock*
typedef std::map<llvm::Function *, std::set<llvm::BasicBlock *>> FuncToBBSetMap;
typedef std::map<llvm::Function *, std::set<llvm::BasicBlock *>>::iterator
    FuncToBBSetMapIt;

// [OS-CFI] BasicBlock* can map to an unsigned long
typedef std::map<llvm::BasicBlock *, unsigned long> BBToHashMap;
typedef std::map<llvm::BasicBlock *, unsigned long>::iterator BBToHashMapIt;

// [OS-CFI] typedef container for CFGs
typedef std::vector<supaCFG *> SUPACFGList;
typedef std::vector<supaCFG *>::iterator SUPACFGListIt;
typedef std::vector<cCFG *> CCFGList;
typedef std::vector<cCFG *>::iterator CCFGListIt;
typedef std::vector<oCFG *> OCFGList;
typedef std::vector<oCFG *>::iterator OCFGListIt;
typedef std::vector<atCFG *> ATCFGList;
typedef std::vector<atCFG *>::iterator ATCFGListIt;
// [OS-CFI] typdef address taken function set
typedef std::set<llvm::Function *> FuncSet;
typedef std::set<llvm::Function *>::iterator FuncSetIt;

class DDAPass : public llvm::ModulePass {
private:
  SUPACFGList supaCFGList; // [OS-CFI] List of SUPA CF-CFG Entry
  CCFGList cCFGList;       // [OS-CFI] List of CS-CFG Entry
  OCFGList oCFGList;       // [OS-CFI] List of OS-CFG Entry
  ATCFGList atCFGList;     // [OS-CFI] List of ADDRTY-CFG Entry
  FuncSet setAddrFunc;     // [OS-CFI] Set of ADDR Functions

  FuncToInstSetMap mapFnCSite; // [OS-CFI] ToDo
  BBToHashMap mapBBHID;        // [OS-CFI] ToDo
  FuncToBBSetMap mapFnBB;      // [OS-CFI] ToDo

public:
  /// Pass ID
  static char ID;
  typedef SCCDetection<SVFG *> SVFGSCC;
  typedef std::set<const SVFGEdge *> SVFGEdgeSet;
  typedef std::vector<PointerAnalysis *> PTAVector;

  DDAPass() : llvm::ModulePass(ID), _pta(NULL), _client(NULL) {}
  ~DDAPass();

  virtual inline void getAnalysisUsage(llvm::AnalysisUsage &au) const {
    // declare your dependencies here.
    /// do not intend to change the IR in this pass,
    au.setPreservesAll();
  }

  virtual inline void *getAdjustedAnalysisPointer(llvm::AnalysisID id) {
    return this;
  }

  /// Interface expose to users of our pointer analysis, given Location infos
  virtual inline llvm::AliasResult alias(const llvm::MemoryLocation &LocA,
                                         const llvm::MemoryLocation &LocB) {
    return alias(LocA.Ptr, LocB.Ptr);
  }

  /// Interface expose to users of our pointer analysis, given Value infos
  virtual llvm::AliasResult alias(const llvm::Value *V1, const llvm::Value *V2);

  /// We start from here
  virtual bool runOnModule(SVFModule module);

  /// We start from here
  virtual bool runOnModule(llvm::Module &module) { return runOnModule(module); }

  /// Select a client
  virtual void selectClient(SVFModule module);

  /// Pass name
  virtual inline llvm::StringRef getPassName() const { return "DDAPass"; }

private:
  unsigned long getHashID(const llvm::Instruction *); // [OS-CFI] return unique
                                                      // id for an instruction
  void computeCFG();  // [OS-CFI] fill out CFG containers
  void dumpSUPACFG(); // [OS-CFI] print SUPA CI-CFG
  void dumpoCFG();    // [OS-CFI] print OS-CFG
  void dumpcCFG();    // [OS-CFI] print CS-CFG
  void dumpatCFG();   // [OS-CFI] print ADDRTY-CFG
  void insertLabelAfterCall(const llvm::Instruction *callInst); // [OS-CFI] ToDo
  void insertIndirectBranch();                                  // [OS-CFI] ToDo
  bool isTypeMatch(const llvm::Instruction *,
                   const llvm::Value *); // [OS-CFI] test the type match between
                                         // sink and source
  void fillEmptyPointsToSet(
      const llvm::Instruction *); // [OS-CFI] use address-taken type match cfg
                                  // for empty points-to set

  /// Print queries' pts
  void printQueryPTS();
  /// Create pointer analysis according to specified kind and analyze the
  /// module.
  void runPointerAnalysis(SVFModule module, u32_t kind);
  /// Initialize queries for DDA
  void answerQueries(PointerAnalysis *pta);
  /// Context insensitive Edge for DDA
  void initCxtInsensitiveEdges(PointerAnalysis *pta, const SVFG *svfg,
                               const SVFGSCC *svfgSCC,
                               SVFGEdgeSet &insensitveEdges);
  /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node
  /// are in the same SCC on the SVFG.
  bool edgeInSVFGSCC(const SVFGSCC *svfgSCC, const SVFGEdge *edge);
  /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node
  /// are in the same SCC on the SVFG.
  bool edgeInCallGraphSCC(PointerAnalysis *pta, const SVFGEdge *edge);

  void collectCxtInsenEdgeForRecur(PointerAnalysis *pta, const SVFG *svfg,
                                   SVFGEdgeSet &insensitveEdges);
  void collectCxtInsenEdgeForVFCycle(PointerAnalysis *pta, const SVFG *svfg,
                                     const SVFGSCC *svfgSCC,
                                     SVFGEdgeSet &insensitveEdges);

  PointerAnalysis *_pta; ///<  pointer analysis to be executed.
  DDAClient *_client;    ///<  DDA client used
};

#endif /* WPA_H_ */
