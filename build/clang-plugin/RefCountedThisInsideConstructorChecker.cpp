/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefCountedThisInsideConstructorChecker.h"
#include "CustomMatchers.h"

void RefCountedThisInsideConstructorChecker::registerMatchers(
    MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      cxxConstructExpr(hasType(isSmartPtrToRefCounted()),
                       hasArgument(0, cxxThisExpr()),
                       hasAncestor(cxxConstructorDecl().bind("constructor")))
          .bind("call"),
      this);
}

void RefCountedThisInsideConstructorChecker::check(
    const MatchFinder::MatchResult &Result) {
  const CXXConstructExpr *Call =
      Result.Nodes.getNodeAs<CXXConstructExpr>("call");

  diag(Call->getBeginLoc(),
       "Refcounting `this` inside the constructor is a footgun, `this` may be "
       "destructed at the end of the constructor unless there's another strong "
       "reference. Consider adding a separate Create function and do the work "
       "there.",
       DiagnosticIDs::Error);
}
