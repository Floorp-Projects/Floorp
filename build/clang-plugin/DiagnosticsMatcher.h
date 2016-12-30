/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DiagnosticsMatcher_h__
#define DiagnosticsMatcher_h__

#include "ChecksIncludes.inc"

class DiagnosticsMatcher {
public:
  DiagnosticsMatcher(CompilerInstance& CI);

  ASTConsumerPtr makeASTConsumer() { return AstMatcher.newASTConsumer(); }

private:
#define CHECK(cls, name) cls cls ## _;
#include "Checks.inc"
#undef CHECK
  MatchFinder AstMatcher;
};

#endif
