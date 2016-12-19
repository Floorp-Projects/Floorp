/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozChecker.h"
#include "CustomTypeAnnotation.h"
#include "Utils.h"

void MozChecker::HandleTranslationUnit(ASTContext &Ctx) {
  TraverseDecl(Ctx.getTranslationUnitDecl());
}

bool MozChecker::hasCustomAnnotation(const Decl *D, const char *Spelling) {
  iterator_range<specific_attr_iterator<AnnotateAttr>> Attrs =
      D->specific_attrs<AnnotateAttr>();

  for (AnnotateAttr *Attr : Attrs) {
    if (Attr->getAnnotation() == Spelling) {
      return true;
    }
  }

  return false;
}

void MozChecker::handleUnusedExprResult(const Stmt *Statement) {
  const Expr *E = dyn_cast_or_null<Expr>(Statement);
  if (E) {
    E = E->IgnoreImplicit(); // Ignore ExprWithCleanup etc. implicit wrappers
    QualType T = E->getType();
    if (MustUse.hasEffectiveAnnotation(T) && !isIgnoredExprForMustUse(E)) {
      unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
          DiagnosticIDs::Error, "Unused value of must-use type %0");

      Diag.Report(E->getLocStart(), ErrorID) << T;
      MustUse.dumpAnnotationReason(Diag, T, E->getLocStart());
    }
  }
}

bool MozChecker::VisitSwitchCase(SwitchCase *Statement) {
  handleUnusedExprResult(Statement->getSubStmt());
  return true;
}

bool MozChecker::VisitCompoundStmt(CompoundStmt *Statement) {
  for (CompoundStmt::body_iterator It = Statement->body_begin(),
                                   E = Statement->body_end();
       It != E; ++It) {
    handleUnusedExprResult(*It);
  }
  return true;
}

bool MozChecker::VisitIfStmt(IfStmt *Statement) {
  handleUnusedExprResult(Statement->getThen());
  handleUnusedExprResult(Statement->getElse());
  return true;
}

bool MozChecker::VisitWhileStmt(WhileStmt *Statement) {
  handleUnusedExprResult(Statement->getBody());
  return true;
}

bool MozChecker::VisitDoStmt(DoStmt *Statement) {
  handleUnusedExprResult(Statement->getBody());
  return true;
}

bool MozChecker::VisitForStmt(ForStmt *Statement) {
  handleUnusedExprResult(Statement->getBody());
  handleUnusedExprResult(Statement->getInit());
  handleUnusedExprResult(Statement->getInc());
  return true;
}

bool MozChecker::VisitBinComma(BinaryOperator *Op) {
  handleUnusedExprResult(Op->getLHS());
  return true;
}
