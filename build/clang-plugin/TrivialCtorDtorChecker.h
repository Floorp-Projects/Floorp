/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrivialCtorDtorChecker_h__
#define TrivialCtorDtorChecker_h__

#include "plugin.h"

class TrivialCtorDtorChecker : public BaseCheck {
public:
  TrivialCtorDtorChecker(StringRef CheckName,
                         ContextType *Context = nullptr)
    : BaseCheck(CheckName, Context) {}
  void registerMatchers(MatchFinder* AstMatcher) override;
  void check(const MatchFinder::MatchResult &Result) override;
};

#endif
