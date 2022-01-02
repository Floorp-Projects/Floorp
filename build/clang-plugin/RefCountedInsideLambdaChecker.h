/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RefCountedInsideLambdaChecker_h__
#define RefCountedInsideLambdaChecker_h__

#include "plugin.h"

class RefCountedInsideLambdaChecker : public BaseCheck {
public:
  RefCountedInsideLambdaChecker(StringRef CheckName,
                                ContextType *Context = nullptr)
      : BaseCheck(CheckName, Context) {}
  void registerMatchers(MatchFinder *AstMatcher) override;
  void check(const MatchFinder::MatchResult &Result) override;

  void emitDiagnostics(SourceLocation Loc, StringRef Name, QualType Type);

private:
  class ThisVisitor : public RecursiveASTVisitor<ThisVisitor> {
  public:
    explicit ThisVisitor(RefCountedInsideLambdaChecker &Checker)
        : Checker(Checker) {}

    bool VisitCXXThisExpr(CXXThisExpr *This);

  private:
    RefCountedInsideLambdaChecker &Checker;
  };
};

#endif
