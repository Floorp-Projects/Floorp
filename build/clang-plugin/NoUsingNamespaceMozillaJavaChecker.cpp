/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoUsingNamespaceMozillaJavaChecker.h"
#include "CustomMatchers.h"

void NoUsingNamespaceMozillaJavaChecker::registerMatchers(
    MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      usingDirectiveDecl(isUsingNamespaceMozillaJava()).bind("directive"),
      this);
}

void NoUsingNamespaceMozillaJavaChecker::check(
    const MatchFinder::MatchResult &Result) {
  const UsingDirectiveDecl *Directive =
      Result.Nodes.getNodeAs<UsingDirectiveDecl>("directive");
  const NamespaceDecl *Namespace = Directive->getNominatedNamespace();

  diag(Directive->getUsingLoc(), "using namespace %0 is forbidden",
       DiagnosticIDs::Error)
      << Namespace->getQualifiedNameAsString();
}
