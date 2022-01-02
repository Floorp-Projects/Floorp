/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryLifetimeBoundChecker.h"
#include "CustomMatchers.h"
#include "clang/Lex/Lexer.h"

void TemporaryLifetimeBoundChecker::registerMatchers(MatchFinder *AstMatcher) {
  // Look for a call to a MOZ_LIFETIME_BOUND member function
  auto isTemporaryLifetimeBoundCall =
      cxxMemberCallExpr(
          onImplicitObjectArgument(anyOf(has(cxxTemporaryObjectExpr()),
                                         has(materializeTemporaryExpr()))),
          callee(functionDecl(isMozTemporaryLifetimeBound())))
          .bind("call");

  // XXX This definitely does not catch everything relevant. In particular, the
  // matching on conditionalOperator would need to be recursive. But it's a
  // start.
  auto hasTemporaryLifetimeBoundCall =
      anyOf(isTemporaryLifetimeBoundCall,
            conditionalOperator(
                anyOf(hasFalseExpression(isTemporaryLifetimeBoundCall),
                      hasTrueExpression(isTemporaryLifetimeBoundCall))));

  AstMatcher->addMatcher(
      returnStmt(hasReturnValue(
                     allOf(exprWithCleanups().bind("expr-with-cleanups"),
                           ignoringParenCasts(hasTemporaryLifetimeBoundCall))))
          .bind("return-stmt"),
      this);

  AstMatcher->addMatcher(
      varDecl(hasType(references(cxxRecordDecl())),
              hasInitializer(
                  allOf(exprWithCleanups(),
                        ignoringParenCasts(hasTemporaryLifetimeBoundCall))))
          .bind("var-decl"),
      this);
}

void TemporaryLifetimeBoundChecker::check(
    const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<CXXMemberCallExpr>("call");
  const auto *ReturnStatement =
      Result.Nodes.getNodeAs<ReturnStmt>("return-stmt");
  const auto *ReferenceVarDecl = Result.Nodes.getNodeAs<VarDecl>("var-decl");

  const char ErrorReturn[] =
      "cannot return result of lifetime-bound function %0 on "
      "temporary of type %1";

  const char ErrorBindToReference[] =
      "cannot bind result of lifetime-bound function %0 on "
      "temporary of type %1 to reference, does not extend lifetime";

  const char NoteCalledFunction[] = "member function declared here";

  // We are either a return statement...
  if (ReturnStatement) {
    const auto *ExprWithCleanups =
        Result.Nodes.getNodeAs<Expr>("expr-with-cleanups");
    if (!ExprWithCleanups->isLValue()) {
      return;
    }

    const auto Range = ReturnStatement->getSourceRange();

    diag(Range.getBegin(), ErrorReturn, DiagnosticIDs::Error)
        << Range << Call->getMethodDecl()
        << Call->getImplicitObjectArgument()
               ->getType()
               .withoutLocalFastQualifiers();
  }

  // ... or a variable declaration that declare a reference
  if (ReferenceVarDecl) {
    const auto Range = ReferenceVarDecl->getSourceRange();

    diag(Range.getBegin(), ErrorBindToReference, DiagnosticIDs::Error)
        << Range << Call->getMethodDecl()
        << Call->getImplicitObjectArgument()
               ->getType()
               .withoutLocalFastQualifiers();
  }

  const auto *MethodDecl = Call->getMethodDecl();
  diag(MethodDecl->getCanonicalDecl()->getLocation(), NoteCalledFunction,
       DiagnosticIDs::Note);
}
