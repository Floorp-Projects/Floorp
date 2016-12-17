/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExplicitImplicitChecker_h__
#define ExplicitImplicitChecker_h__

#include "plugin.h"

class ExplicitImplicitChecker : public MatchFinder::MatchCallback {
public:
  void registerMatcher(MatchFinder& AstMatcher);
  virtual void run(const MatchFinder::MatchResult &Result);
};

#endif
