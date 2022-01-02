/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefCountedInsideLambdaChecker.h"
#include "CustomMatchers.h"

RefCountedMap RefCountedClasses;

void RefCountedInsideLambdaChecker::registerMatchers(MatchFinder *AstMatcher) {
  // We want to reject any code which captures a pointer to an object of a
  // refcounted type, and then lets that value escape. As a primitive analysis,
  // we reject any occurances of the lambda as a template parameter to a class
  // (which could allow it to escape), as well as any presence of such a lambda
  // in a return value (either from lambdas, or in c++14, auto functions).
  //
  // We check these lambdas' capture lists for raw pointers to refcounted types.
  AstMatcher->addMatcher(functionDecl(returns(recordType(hasDeclaration(
                             cxxRecordDecl(isLambdaDecl()).bind("decl"))))),
                         this);
  AstMatcher->addMatcher(lambdaExpr().bind("lambdaExpr"), this);
  AstMatcher->addMatcher(
      classTemplateSpecializationDecl(
          hasAnyTemplateArgument(refersToType(recordType(
              hasDeclaration(cxxRecordDecl(isLambdaDecl()).bind("decl")))))),
      this);
}

void RefCountedInsideLambdaChecker::emitDiagnostics(SourceLocation Loc,
                                                    StringRef Name,
                                                    QualType Type) {
  diag(Loc,
       "Refcounted variable '%0' of type %1 cannot be captured by a lambda",
       DiagnosticIDs::Error)
      << Name << Type;
  diag(Loc, "Please consider using a smart pointer", DiagnosticIDs::Note);
}

static bool IsKnownLive(const VarDecl *Var) {
  const Stmt *Init = Var->getInit();
  if (!Init) {
    return false;
  }
  if (auto *Call = dyn_cast<CallExpr>(Init)) {
    const FunctionDecl *Callee = Call->getDirectCallee();
    return Callee && Callee->getName() == "MOZ_KnownLive";
  }
  return false;
}

void RefCountedInsideLambdaChecker::check(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const CXXRecordDecl *> CheckedDecls;

  const CXXRecordDecl *Lambda = Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  if (const LambdaExpr *OuterLambda =
          Result.Nodes.getNodeAs<LambdaExpr>("lambdaExpr")) {
    const CXXMethodDecl *OpCall = OuterLambda->getCallOperator();
    QualType ReturnTy = OpCall->getReturnType();
    if (const CXXRecordDecl *Record = ReturnTy->getAsCXXRecordDecl()) {
      Lambda = Record;
    }
  }

  if (!Lambda || !Lambda->isLambda()) {
    return;
  }

  // Don't report errors on the same declarations more than once.
  if (CheckedDecls.count(Lambda)) {
    return;
  }
  CheckedDecls.insert(Lambda);

  bool StrongRefToThisCaptured = false;

  for (const LambdaCapture &Capture : Lambda->captures()) {
    // Check if any of the captures are ByRef. If they are, we have nothing to
    // report, as it's OK to capture raw pointers to refcounted objects so long
    // as the Lambda doesn't escape the current scope, which is required by
    // ByRef captures already.
    if (Capture.getCaptureKind() == LCK_ByRef) {
      return;
    }

    // Check if this capture is byvalue, and captures a strong reference to
    // this.
    // XXX: Do we want to make sure that this type which we are capturing is a
    // "Smart Pointer" somehow?
    if (!StrongRefToThisCaptured && Capture.capturesVariable() &&
        Capture.getCaptureKind() == LCK_ByCopy) {
      const VarDecl *Var = Capture.getCapturedVar();
      if (Var->hasInit()) {
        const Stmt *Init = Var->getInit();

        // Ignore single argument constructors, and trivial nodes.
        while (true) {
          auto NewInit = IgnoreTrivials(Init);
          if (auto ConstructExpr = dyn_cast<CXXConstructExpr>(NewInit)) {
            if (ConstructExpr->getNumArgs() == 1) {
              NewInit = ConstructExpr->getArg(0);
            }
          }
          if (Init == NewInit) {
            break;
          }
          Init = NewInit;
        }

        if (isa<CXXThisExpr>(Init)) {
          StrongRefToThisCaptured = true;
        }
      }
    }
  }

  // Now we can go through and produce errors for any captured variables or this
  // pointers.
  for (const LambdaCapture &Capture : Lambda->captures()) {
    if (Capture.capturesVariable()) {
      const VarDecl *Var = Capture.getCapturedVar();
      QualType Pointee = Var->getType()->getPointeeType();
      if (!Pointee.isNull() && isClassRefCounted(Pointee) &&
          !IsKnownLive(Var)) {
        emitDiagnostics(Capture.getLocation(), Var->getName(), Pointee);
        return;
      }
    }

    // The situation with captures of `this` is more complex. All captures of
    // `this` look the same-ish (they are LCK_This). We want to complain about
    // captures of `this` where `this` is a refcounted type, and the capture is
    // actually used in the body of the lambda (if the capture isn't used, then
    // we don't care, because it's only being captured in order to give access
    // to private methods).
    //
    // In addition, we don't complain about this, even if it is used, if it was
    // captured implicitly when the LambdaCaptureDefault was LCD_ByRef, as that
    // expresses the intent that the lambda won't leave the enclosing scope.
    bool ImplicitByRefDefaultedCapture =
        Capture.isImplicit() && Lambda->getLambdaCaptureDefault() == LCD_ByRef;
    if (Capture.capturesThis() && !ImplicitByRefDefaultedCapture &&
        !StrongRefToThisCaptured) {
      ThisVisitor V(*this);
      bool NotAborted = V.TraverseDecl(
          const_cast<CXXMethodDecl *>(Lambda->getLambdaCallOperator()));
      if (!NotAborted) {
        return;
      }
    }
  }
}

bool RefCountedInsideLambdaChecker::ThisVisitor::VisitCXXThisExpr(
    CXXThisExpr *This) {
  QualType Pointee = This->getType()->getPointeeType();
  if (!Pointee.isNull() && isClassRefCounted(Pointee)) {
    Checker.emitDiagnostics(This->getBeginLoc(), "this", Pointee);
    return false;
  }

  return true;
}
