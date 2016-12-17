/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArithmeticArgChecker_h__
#define ArithmeticArgChecker_h__

#include "plugin.h"

class ArithmeticArgChecker : public MatchFinder::MatchCallback {
public:
  void registerMatcher(MatchFinder& AstMatcher);
  virtual void run(const MatchFinder::MatchResult &Result);
};

#endif
