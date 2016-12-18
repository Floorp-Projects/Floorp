/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonMemMovableMemberChecker.h"
#include "CustomMatchers.h"

MemMoveAnnotation NonMemMovable = MemMoveAnnotation();

void NonMemMovableMemberChecker::registerMatchers(MatchFinder* AstMatcher) {
  // Handle non-mem-movable members
  AstMatcher->addMatcher(
      cxxRecordDecl(needsMemMovableMembers())
          .bind("decl"),
      this);
}

void NonMemMovableMemberChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error =
      "class %0 cannot have non-memmovable member %1 of type %2";

  // Get the specialization
  const CXXRecordDecl* Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  // Report an error for every member which is non-memmovable
  for (const FieldDecl *Field : Declaration->fields()) {
    QualType Type = Field->getType();
    if (NonMemMovable.hasEffectiveAnnotation(Type)) {
      diag(Field->getLocation(), Error, DiagnosticIDs::Error)
        << Declaration
        << Field
        << Type;
      NonMemMovable.dumpAnnotationReason(*this, Type, Declaration->getLocation());
    }
  }
}
