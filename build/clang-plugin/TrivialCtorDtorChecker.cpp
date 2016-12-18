/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrivialCtorDtorChecker.h"
#include "CustomMatchers.h"

void TrivialCtorDtorChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(cxxRecordDecl(hasTrivialCtorDtor()).bind("node"),
                         this);
}

void TrivialCtorDtorChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error =
      "class %0 must have trivial constructors and destructors";
  const CXXRecordDecl *Node = Result.Nodes.getNodeAs<CXXRecordDecl>("node");

  // We need to accept non-constexpr trivial constructors as well. This occurs
  // when a struct contains pod members, which will not be initialized. As
  // constexpr values are initialized, the constructor is non-constexpr.
  bool BadCtor = !(Node->hasConstexprDefaultConstructor() ||
                   Node->hasTrivialDefaultConstructor());
  bool BadDtor = !Node->hasTrivialDestructor();
  if (BadCtor || BadDtor)
    diag(Node->getLocStart(), Error, DiagnosticIDs::Error) << Node;
}
