/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScopeChecker.h"
#include "CustomMatchers.h"

void ScopeChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(varDecl().bind("node"), this);
  AstMatcher.addMatcher(cxxNewExpr().bind("node"), this);
  AstMatcher.addMatcher(materializeTemporaryExpr().bind("node"), this);
  AstMatcher.addMatcher(
      callExpr(callee(functionDecl(heapAllocator()))).bind("node"),
      this);
  AstMatcher.addMatcher(parmVarDecl().bind("parm_vardecl"), this);
}

// These enum variants determine whether an allocation has occured in the code.
enum AllocationVariety {
  AV_None,
  AV_Global,
  AV_Automatic,
  AV_Temporary,
  AV_Heap,
};

// XXX Currently the Decl* in the AutomaticTemporaryMap is unused, but it
// probably will be used at some point in the future, in order to produce better
// error messages.
typedef DenseMap<const MaterializeTemporaryExpr *, const Decl *>
    AutomaticTemporaryMap;
AutomaticTemporaryMap AutomaticTemporaries;

void ScopeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();

  // There are a variety of different reasons why something could be allocated
  AllocationVariety Variety = AV_None;
  SourceLocation Loc;
  QualType T;

  if (const ParmVarDecl *D =
          Result.Nodes.getNodeAs<ParmVarDecl>("parm_vardecl")) {
    if (D->hasUnparsedDefaultArg() || D->hasUninstantiatedDefaultArg()) {
      return;
    }
    if (const Expr *Default = D->getDefaultArg()) {
      if (const MaterializeTemporaryExpr *E =
              dyn_cast<MaterializeTemporaryExpr>(Default)) {
        // We have just found a ParmVarDecl which has, as its default argument,
        // a MaterializeTemporaryExpr. We mark that MaterializeTemporaryExpr as
        // automatic, by adding it to the AutomaticTemporaryMap.
        // Reporting on this type will occur when the MaterializeTemporaryExpr
        // is matched against.
        AutomaticTemporaries[E] = D;
      }
    }
    return;
  }

  // Determine the type of allocation which we detected
  if (const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("node")) {
    if (D->hasGlobalStorage()) {
      Variety = AV_Global;
    } else {
      Variety = AV_Automatic;
    }
    T = D->getType();
    Loc = D->getLocStart();
  } else if (const CXXNewExpr *E = Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // New allocates things on the heap.
    // We don't consider placement new to do anything, as it doesn't actually
    // allocate the storage, and thus gives us no useful information.
    if (!isPlacementNew(E)) {
      Variety = AV_Heap;
      T = E->getAllocatedType();
      Loc = E->getLocStart();
    }
  } else if (const MaterializeTemporaryExpr *E =
                 Result.Nodes.getNodeAs<MaterializeTemporaryExpr>("node")) {
    // Temporaries can actually have varying storage durations, due to temporary
    // lifetime extension. We consider the allocation variety of this temporary
    // to be the same as the allocation variety of its lifetime.

    // XXX We maybe should mark these lifetimes as being due to a temporary
    // which has had its lifetime extended, to improve the error messages.
    switch (E->getStorageDuration()) {
    case SD_FullExpression: {
      // Check if this temporary is allocated as a default argument!
      // if it is, we want to pretend that it is automatic.
      AutomaticTemporaryMap::iterator AutomaticTemporary =
          AutomaticTemporaries.find(E);
      if (AutomaticTemporary != AutomaticTemporaries.end()) {
        Variety = AV_Automatic;
      } else {
        Variety = AV_Temporary;
      }
    } break;
    case SD_Automatic:
      Variety = AV_Automatic;
      break;
    case SD_Thread:
    case SD_Static:
      Variety = AV_Global;
      break;
    case SD_Dynamic:
      assert(false && "I don't think that this ever should occur...");
      Variety = AV_Heap;
      break;
    }
    T = E->getType().getUnqualifiedType();
    Loc = E->getLocStart();
  } else if (const CallExpr *E = Result.Nodes.getNodeAs<CallExpr>("node")) {
    T = E->getType()->getPointeeType();
    if (!T.isNull()) {
      // This will always allocate on the heap, as the heapAllocator() check
      // was made in the matcher
      Variety = AV_Heap;
      Loc = E->getLocStart();
    }
  }

  // Error messages for incorrect allocations.
  unsigned StackID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid on the stack");
  unsigned GlobalID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid as global");
  unsigned HeapID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid on the heap");
  unsigned NonHeapID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 is not valid on the heap");
  unsigned NonTemporaryID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 is not valid in a temporary");

  unsigned StackNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "value incorrectly allocated in an automatic variable");
  unsigned GlobalNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated in a global variable");
  unsigned HeapNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated on the heap");
  unsigned TemporaryNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated in a temporary");

  // Report errors depending on the annotations on the input types.
  switch (Variety) {
  case AV_None:
    return;

  case AV_Global:
    StackClass.reportErrorIfPresent(Diag, T, Loc, StackID, GlobalNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, GlobalNoteID);
    break;

  case AV_Automatic:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, StackNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, StackNoteID);
    break;

  case AV_Temporary:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, TemporaryNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, TemporaryNoteID);
    NonTemporaryClass.reportErrorIfPresent(Diag, T, Loc, NonTemporaryID,
                                           TemporaryNoteID);
    break;

  case AV_Heap:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, HeapNoteID);
    StackClass.reportErrorIfPresent(Diag, T, Loc, StackID, HeapNoteID);
    NonHeapClass.reportErrorIfPresent(Diag, T, Loc, NonHeapID, HeapNoteID);
    break;
  }
}
