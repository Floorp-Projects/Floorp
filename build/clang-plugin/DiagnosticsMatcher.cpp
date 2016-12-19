/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DiagnosticsMatcher.h"

DiagnosticsMatcher::DiagnosticsMatcher(CompilerInstance& CI) :
#define CHECK(cls, name) cls ## _(name),
#include "Checks.inc"
#undef CHECK
  AstMatcher()
{
#define CHECK(cls, name) cls ## _.registerMatchers(&AstMatcher); \
                         cls ## _.registerPPCallbacks(CI);
#include "Checks.inc"
#undef CHECK
}
