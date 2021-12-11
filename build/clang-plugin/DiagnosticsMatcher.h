/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DiagnosticsMatcher_h__
#define DiagnosticsMatcher_h__

#include "ChecksIncludes.inc"
#include "external/ExternalIncludes.inc"
#ifdef MOZ_CLANG_PLUGIN_ALPHA
#include "alpha/AlphaIncludes.inc"
#endif

class DiagnosticsMatcher {
public:
  DiagnosticsMatcher(CompilerInstance &CI);

  ASTConsumerPtr makeASTConsumer() { return AstMatcher.newASTConsumer(); }

private:
#define CHECK(cls, name) cls cls##_{name};
#include "Checks.inc"
#include "external/ExternalChecks.inc"
#ifdef MOZ_CLANG_PLUGIN_ALPHA
#include "alpha/AlphaChecks.inc"
#endif
#undef CHECK
  MatchFinder AstMatcher;
};

#endif
