/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonMemMovableMemberChecker.h"
#include "CustomMatchers.h"

MemMoveAnnotation NonMemMovable = MemMoveAnnotation();

void NonMemMovableMemberChecker::registerMatcher(MatchFinder& AstMatcher) {
  // Handle non-mem-movable members
  AstMatcher.addMatcher(
      cxxRecordDecl(needsMemMovableMembers())
          .bind("decl"),
      this);
}

void NonMemMovableMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "class %0 cannot have non-memmovable member %1 of type %2");

  // Get the specialization
  const CXXRecordDecl* Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  // Report an error for every member which is non-memmovable
  for (const FieldDecl *Field : Declaration->fields()) {
    QualType Type = Field->getType();
    if (NonMemMovable.hasEffectiveAnnotation(Type)) {
      Diag.Report(Field->getLocation(), ErrorID) << Declaration
                                                 << Field
                                                 << Type;
      NonMemMovable.dumpAnnotationReason(Diag, Type, Declaration->getLocation());
    }
  }
}
