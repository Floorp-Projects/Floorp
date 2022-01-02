/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoNewThreadsChecker.h"
#include "CustomMatchers.h"

void NoNewThreadsChecker::registerMatchers(MatchFinder *AstMatcher) {
  // The checker looks for:
  // -Instances of NS_NewNamedThread that aren't in allowed files
  // -Instances of NS_NewNamedThread that use names that aren't recognized
  AstMatcher->addMatcher(
      callExpr(allOf(isFirstParty(),
                     callee(functionDecl(hasName("NS_NewNamedThread"))),
                     unless(isInAllowlistForThreads())))
          .bind("funcCall"),
      this);
}

void NoNewThreadsChecker::check(const MatchFinder::MatchResult &Result) {
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (FuncCall) {
    diag(FuncCall->getBeginLoc(),
         "Thread name not recognized. Please use the background thread pool.",
         DiagnosticIDs::Error)
        << FuncCall->getDirectCallee()->getName();
    diag(
        FuncCall->getBeginLoc(),
        "NS_NewNamedThread has been deprecated in favor of background "
        "task dispatch via NS_DispatchBackgroundTask and "
        "NS_CreateBackgroundTaskQueue. If you must create a new ad-hoc thread, "
        "have your thread name added to ThreadAllows.txt.",
        DiagnosticIDs::Note);
  }
}
