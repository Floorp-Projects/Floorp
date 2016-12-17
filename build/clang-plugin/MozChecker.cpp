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

bool MozChecker::VisitCXXRecordDecl(CXXRecordDecl *D) {
  // We need definitions, not declarations
  if (!D->isThisDeclarationADefinition())
    return true;

  // Look through all of our immediate bases to find methods that need to be
  // overridden
  typedef std::vector<CXXMethodDecl *> OverridesVector;
  OverridesVector MustOverrides;
  for (CXXRecordDecl::base_class_iterator Base = D->bases_begin(),
                                          E = D->bases_end();
       Base != E; ++Base) {
    // The base is either a class (CXXRecordDecl) or it's a templated class...
    CXXRecordDecl *Parent = Base->getType()
                                .getDesugaredType(D->getASTContext())
                                ->getAsCXXRecordDecl();
    // The parent might not be resolved to a type yet. In this case, we can't
    // do any checking here. For complete correctness, we should visit
    // template instantiations, but this case is likely to be rare, so we will
    // ignore it until it becomes important.
    if (!Parent) {
      continue;
    }
    Parent = Parent->getDefinition();
    for (CXXRecordDecl::method_iterator M = Parent->method_begin();
         M != Parent->method_end(); ++M) {
      if (hasCustomAnnotation(*M, "moz_must_override"))
        MustOverrides.push_back(*M);
    }
  }

  for (OverridesVector::iterator It = MustOverrides.begin();
       It != MustOverrides.end(); ++It) {
    bool Overridden = false;
    for (CXXRecordDecl::method_iterator M = D->method_begin();
         !Overridden && M != D->method_end(); ++M) {
      // The way that Clang checks if a method M overrides its parent method
      // is if the method has the same name but would not overload.
      if (getNameChecked(M) == getNameChecked(*It) &&
          !CI.getSema().IsOverload(*M, (*It), false)) {
        Overridden = true;
        break;
      }
    }
    if (!Overridden) {
      unsigned OverrideID = Diag.getDiagnosticIDs()->getCustomDiagID(
          DiagnosticIDs::Error, "%0 must override %1");
      unsigned OverrideNote = Diag.getDiagnosticIDs()->getCustomDiagID(
          DiagnosticIDs::Note, "function to override is here");
      Diag.Report(D->getLocation(), OverrideID) << D->getDeclName()
                                                << (*It)->getDeclName();
      Diag.Report((*It)->getLocation(), OverrideNote);
    }
  }

  return true;
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
