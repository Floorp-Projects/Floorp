/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrivialDtorChecker.h"
#include "CustomMatchers.h"

void TrivialDtorChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(cxxRecordDecl(hasTrivialDtor()).bind("node"),
                         this);
}

void TrivialDtorChecker::check(const MatchFinder::MatchResult &Result) {
  const char *Error = "class %0 must have a trivial destructor";
  const CXXRecordDecl *Node = Result.Nodes.getNodeAs<CXXRecordDecl>("node");

  if (!Node->hasDefinition()) {
    return;
  }

  bool BadDtor = !Node->hasTrivialDestructor();
  if (BadDtor)
    diag(Node->getBeginLoc(), Error, DiagnosticIDs::Error) << Node;
}
