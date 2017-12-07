/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParamTraitsEnumChecker.h"
#include "CustomMatchers.h"

void ParamTraitsEnumChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
    classTemplateSpecializationDecl(hasName("ParamTraits")).bind("decl"), this);
}

void ParamTraitsEnumChecker::check(const MatchFinder::MatchResult &Result) {
  const ClassTemplateSpecializationDecl *Decl =
    Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("decl");

  for (auto& Inner : Decl->decls()) {
    if (auto* Def = dyn_cast<TypedefDecl>(Inner)) {
      QualType UnderlyingType = Def->getUnderlyingType();
      QualType CanonicalType = UnderlyingType.getCanonicalType();

      const clang::Type *TypePtr = CanonicalType.getTypePtrOrNull();
      if (!TypePtr) {
        return;
      }

      if (TypePtr->isEnumeralType()) {
        diag(Decl->getLocStart(),
             "Custom ParamTraits implementation for an enum type",
             DiagnosticIDs::Error);
        diag(Decl->getLocStart(),
             "Please use a helper class for example ContiguousEnumSerializer",
             DiagnosticIDs::Note);
      }
    }
  }
}
