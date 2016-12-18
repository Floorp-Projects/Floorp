/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef CLANG_TIDY

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "ChecksIncludes.inc"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {

class MozillaModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
#define CHECK(cls, name) CheckFactories.registerCheck<cls>("mozilla-" name);
#include "Checks.inc"
#undef CHECK
  }
};

// Register the MozillaTidyModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<MozillaModule> X("mozilla-module",
                                                     "Adds Mozilla lint checks.");

} // namespace tidy
} // namespace clang

// This anchor is used to force the linker to link in the generated object file
// and thus register the MozillaModule.
volatile int MozillaModuleAnchorSource = 0;

#endif
