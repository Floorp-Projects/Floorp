/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MustOverrideChecker.h"
#include "CustomMatchers.h"

void MustOverrideChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(cxxRecordDecl(isDefinition()).bind("class"), this);
}

void MustOverrideChecker::registerPPCallbacks(CompilerInstance& CI) {
  this->CI = &CI;
}

void MustOverrideChecker::check(
    const MatchFinder::MatchResult &Result) {
  auto D = Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  // Look through all of our immediate bases to find methods that need to be
  // overridden
  typedef std::vector<CXXMethodDecl *> OverridesVector;
  OverridesVector MustOverrides;
  for (const auto& Base : D->bases()) {
    // The base is either a class (CXXRecordDecl) or it's a templated class...
    CXXRecordDecl *Parent = Base.getType()
                                .getDesugaredType(D->getASTContext())
                                ->getAsCXXRecordDecl();
    // The parent might not be resolved to a type yet. In this case, we can't
    // do any checking here. For complete correctness, we should visit
    // template instantiations, but this case is likely to be rare, so we will
    // ignore it until it becomes important.
    if (!Parent) {
      continue;
    }
    Parent = Parent->getDefinition();
    for (const auto& M : Parent->methods()) {
      if (hasCustomAnnotation(M, "moz_must_override"))
        MustOverrides.push_back(M);
    }
  }

  for (auto& O : MustOverrides) {
    bool Overridden = false;
    for (const auto& M : D->methods()) {
      // The way that Clang checks if a method M overrides its parent method
      // is if the method has the same name but would not overload.
      if (getNameChecked(M) == getNameChecked(O) &&
          !CI->getSema().IsOverload(M, O, false)) {
        Overridden = true;
        break;
      }
    }
    if (!Overridden) {
      diag(D->getLocation(), "%0 must override %1",
           DiagnosticIDs::Error) << D->getDeclName()
                                 << O->getDeclName();
      diag(O->getLocation(), "function to override is here",
           DiagnosticIDs::Note);
    }
  }
}
