/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadLibraryUsageChecker.h"
#include "CustomMatchers.h"

// On MacOS the filesystem is UTF-8, on linux the canonical filename is 8-bit
// string. On Windows data loss conversion will occur. This checker restricts
// the use of ASCII file functions for loading libraries.

void LoadLibraryUsageChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      callExpr(
          allOf(isFirstParty(),
                callee(functionDecl(anyOf(
                    allOf(isInSystemHeader(), anyOf(hasName("LoadLibraryA"),
                                                    hasName("LoadLibraryExA"))),
                    hasName("PR_LoadLibrary")))),
                unless(hasArgument(0, stringLiteral()))))
          .bind("funcCall"),
      this);
}

void LoadLibraryUsageChecker::check(const MatchFinder::MatchResult &Result) {
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (FuncCall) {
    diag(FuncCall->getLocStart(),
         "Usage of ASCII file functions (such as %0) is forbidden.",
         DiagnosticIDs::Error)
        << FuncCall->getDirectCallee()->getName();
  }
}
