/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MustReturnFromCallerChecker_h__
#define MustReturnFromCallerChecker_h__

#include "RecurseGuard.h"
#include "StmtToBlockMap.h"
#include "Utils.h"
#include "plugin.h"

class MustReturnFromCallerChecker : public BaseCheck {
public:
  MustReturnFromCallerChecker(StringRef CheckName,
                              ContextType *Context = nullptr)
      : BaseCheck(CheckName, Context) {}
  void registerMatchers(MatchFinder *AstMatcher) override;
  void check(const MatchFinder::MatchResult &Result) override;

private:
  bool immediatelyReturns(RecurseGuard<const CFGBlock *> Block,
                          ASTContext *TheContext, size_t FromIdx);
};

#endif
