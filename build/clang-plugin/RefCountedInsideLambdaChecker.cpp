/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefCountedInsideLambdaChecker.h"
#include "CustomMatchers.h"

RefCountedMap RefCountedClasses;

void RefCountedInsideLambdaChecker::registerMatchers(MatchFinder* AstMatcher) {
  // We want to reject any code which captures a pointer to an object of a
  // refcounted type, and then lets that value escape. As a primitive analysis,
  // we reject any occurances of the lambda as a template parameter to a class
  // (which could allow it to escape), as well as any presence of such a lambda
  // in a return value (either from lambdas, or in c++14, auto functions).
  //
  // We check these lambdas' capture lists for raw pointers to refcounted types.
  AstMatcher->addMatcher(
      functionDecl(returns(recordType(hasDeclaration(cxxRecordDecl(
        isLambdaDecl()).bind("decl"))))),
      this);
  AstMatcher->addMatcher(lambdaExpr().bind("lambdaExpr"),
      this);
  AstMatcher->addMatcher(
      classTemplateSpecializationDecl(hasAnyTemplateArgument(refersToType(
        recordType(hasDeclaration(cxxRecordDecl(
          isLambdaDecl()).bind("decl")))))),
      this);
}

void RefCountedInsideLambdaChecker::check(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const CXXRecordDecl*> CheckedDecls;

  const char* Error =
      "Refcounted variable %0 of type %1 cannot be captured by a lambda";
  const char* Note =
      "Please consider using a smart pointer";

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

  for (const LambdaCapture Capture : Lambda->captures()) {
    if (Capture.capturesVariable() && Capture.getCaptureKind() != LCK_ByRef) {
      QualType Pointee = Capture.getCapturedVar()->getType()->getPointeeType();

      if (!Pointee.isNull() && isClassRefCounted(Pointee)) {
        diag(Capture.getLocation(), Error,
             DiagnosticIDs::Error) << Capture.getCapturedVar()
                                   << Pointee;
        diag(Capture.getLocation(), Note,
             DiagnosticIDs::Note);
        return;
      }
    }
  }
}
