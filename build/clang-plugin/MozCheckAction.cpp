/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plugin.h"
#include "DiagnosticsMatcher.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI,
                                   StringRef FileName) override {
    void* Buffer = CI.getASTContext().Allocate<DiagnosticsMatcher>();
    auto Matcher = new(Buffer) DiagnosticsMatcher(CI);
    return Matcher->makeASTConsumer();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    return true;
  }
};

static FrontendPluginRegistry::Add<MozCheckAction> X("moz-check",
                                                     "check moz action");
