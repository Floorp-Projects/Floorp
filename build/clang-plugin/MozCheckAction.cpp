/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozChecker.h"

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI,
                                   StringRef FileName) override {
#if CLANG_VERSION_FULL >= 306
    std::unique_ptr<MozChecker> Checker(llvm::make_unique<MozChecker>(CI));
    ASTConsumerPtr Other(Checker->getOtherConsumer());

    std::vector<ASTConsumerPtr> Consumers;
    Consumers.push_back(std::move(Checker));
    Consumers.push_back(std::move(Other));
    return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
#else
    MozChecker *Checker = new MozChecker(CI);

    ASTConsumer *Consumers[] = {Checker, Checker->getOtherConsumer()};
    return new MultiplexConsumer(Consumers);
#endif
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    return true;
  }
};

static FrontendPluginRegistry::Add<MozCheckAction> X("moz-check",
                                                     "check moz action");
