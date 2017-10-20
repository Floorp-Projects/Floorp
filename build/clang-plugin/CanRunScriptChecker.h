/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CanRunScriptChecker_h__
#define CanRunScriptChecker_h__

#include "plugin.h"
#include <unordered_set>

class CanRunScriptChecker : public BaseCheck {
public:
  CanRunScriptChecker(StringRef CheckName, ContextType *Context = nullptr)
      : BaseCheck(CheckName, Context) {}
  void registerMatchers(MatchFinder *AstMatcher) override;
  void check(const MatchFinder::MatchResult &Result) override;

  // Simply initialize the can-run-script function set at the beginning of each
  // translation unit.
  void onStartOfTranslationUnit() override;

private:
  /// Runs the inner matcher on the AST to find all the can-run-script
  /// functions using custom rules (not only the annotation).
  void buildFuncSet(ASTContext *Context);

  bool IsFuncSetBuilt;
  std::unordered_set<const FunctionDecl *> CanRunScriptFuncs;
};

#endif
