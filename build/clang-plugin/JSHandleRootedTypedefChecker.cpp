/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSHandleRootedTypedefChecker.h"
#include "CustomMatchers.h"

void JSHandleRootedTypedefChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      declaratorDecl(isUsingJSHandleRootedTypedef(), isNotSpiderMonkey())
          .bind("declaratorDecl"),
      this);
}

std::string getReplacement(std::string TypeName) {
  for (auto &pair : JSHandleRootedTypedefMap) {
    if (!TypeName.compare(pair[0])) {
      return pair[1];
    }
  }
  llvm_unreachable("Unexpected type name");
}

void JSHandleRootedTypedefChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char *Error = "The fully qualified types are preferred over the "
                        "shorthand typedefs for JS::Handle/JS::Rooted types "
                        "outside SpiderMonkey.";

  const DeclaratorDecl *Declarator =
      Result.Nodes.getNodeAs<DeclaratorDecl>("declaratorDecl");

  std::string Replacement = getReplacement(Declarator->getType().getAsString());
  diag(Declarator->getBeginLoc(), Error, DiagnosticIDs::Error)
      << FixItHint::CreateReplacement(
             Declarator->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
             Replacement);
}
