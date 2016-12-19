/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MozChecker_h__
#define MozChecker_h__

#include "DiagnosticsMatcher.h"

class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsEngine &Diag;
  DiagnosticsMatcher Matcher;

public:
  MozChecker(CompilerInstance &CI) :
    Diag(CI.getDiagnostics()), Matcher(CI) {}
  virtual ~MozChecker() {}

  ASTConsumerPtr getOtherConsumer() { return Matcher.makeASTConsumer(); }

  virtual void HandleTranslationUnit(ASTContext &Ctx) override;

  static bool hasCustomAnnotation(const Decl *D, const char *Spelling);

  void handleUnusedExprResult(const Stmt *Statement);

  bool VisitSwitchCase(SwitchCase *Statement);
  bool VisitCompoundStmt(CompoundStmt *Statement);
  bool VisitIfStmt(IfStmt *Statement);
  bool VisitWhileStmt(WhileStmt *Statement);
  bool VisitDoStmt(DoStmt *Statement);
  bool VisitForStmt(ForStmt *Statement);
  bool VisitBinComma(BinaryOperator *Op);
};

#endif
