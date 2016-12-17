/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoDuplicateRefCntMemberChecker.h"
#include "CustomMatchers.h"

void NoDuplicateRefCntMemberChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(cxxRecordDecl().bind("decl"), this);
}

void NoDuplicateRefCntMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  const CXXRecordDecl *D = Result.Nodes.getNodeAs<CXXRecordDecl>("decl");
  const FieldDecl *RefCntMember = getClassRefCntMember(D);
  const FieldDecl *FoundRefCntBase = nullptr;

  if (!D->hasDefinition())
    return;
  D = D->getDefinition();

  // If we don't have an mRefCnt member, and we have less than 2 superclasses,
  // we don't have to run this loop, as neither case will ever apply.
  if (!RefCntMember && D->getNumBases() < 2) {
    return;
  }

  // Check every superclass for whether it has a base with a refcnt member, and
  // warn for those which do
  for (auto &Base : D->bases()) {
    // Determine if this base class has an mRefCnt member
    const FieldDecl *BaseRefCntMember = getBaseRefCntMember(Base.getType());

    if (BaseRefCntMember) {
      if (RefCntMember) {
        // We have an mRefCnt, and superclass has an mRefCnt
        unsigned Error = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error,
            "Refcounted record %0 has multiple mRefCnt members");
        unsigned Note1 = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note, "Superclass %0 also has an mRefCnt member");
        unsigned Note2 = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note,
            "Consider using the _INHERITED macros for AddRef and Release here");

        Diag.Report(D->getLocStart(), Error) << D;
        Diag.Report(BaseRefCntMember->getLocStart(), Note1)
          << BaseRefCntMember->getParent();
        Diag.Report(RefCntMember->getLocStart(), Note2);
      }

      if (FoundRefCntBase) {
        unsigned Error = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error,
            "Refcounted record %0 has multiple superclasses with mRefCnt members");
        unsigned Note = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note,
            "Superclass %0 has an mRefCnt member");

        // superclass has mRefCnt, and another superclass also has an mRefCnt
        Diag.Report(D->getLocStart(), Error) << D;
        Diag.Report(BaseRefCntMember->getLocStart(), Note)
          << BaseRefCntMember->getParent();
        Diag.Report(FoundRefCntBase->getLocStart(), Note)
          << FoundRefCntBase->getParent();
      }

      // Record that we've found a base with a mRefCnt member
      FoundRefCntBase = BaseRefCntMember;
    }
  }
}
