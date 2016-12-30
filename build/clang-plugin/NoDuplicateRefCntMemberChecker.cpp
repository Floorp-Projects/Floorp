/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoDuplicateRefCntMemberChecker.h"
#include "CustomMatchers.h"

void NoDuplicateRefCntMemberChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(cxxRecordDecl().bind("decl"), this);
}

void NoDuplicateRefCntMemberChecker::check(
    const MatchFinder::MatchResult &Result) {
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
        const char* Error =
            "Refcounted record %0 has multiple mRefCnt members";
        const char* Note1 =
            "Superclass %0 also has an mRefCnt member";
        const char* Note2 =
            "Consider using the _INHERITED macros for AddRef and Release here";

        diag(D->getLocStart(), Error, DiagnosticIDs::Error) << D;
        diag(BaseRefCntMember->getLocStart(), Note1, DiagnosticIDs::Note)
          << BaseRefCntMember->getParent();
        diag(RefCntMember->getLocStart(), Note2, DiagnosticIDs::Note);
      }

      if (FoundRefCntBase) {
        const char* Error =
            "Refcounted record %0 has multiple superclasses with mRefCnt members";
        const char* Note =
            "Superclass %0 has an mRefCnt member";

        // superclass has mRefCnt, and another superclass also has an mRefCnt
        diag(D->getLocStart(), Error, DiagnosticIDs::Error) << D;
        diag(BaseRefCntMember->getLocStart(), Note, DiagnosticIDs::Note)
          << BaseRefCntMember->getParent();
        diag(FoundRefCntBase->getLocStart(), Note, DiagnosticIDs::Note)
          << FoundRefCntBase->getParent();
      }

      // Record that we've found a base with a mRefCnt member
      FoundRefCntBase = BaseRefCntMember;
    }
  }
}
