/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DanglingOnTemporaryChecker.h"
#include "CustomMatchers.h"
#include "VariableUsageHelpers.h"

void DanglingOnTemporaryChecker::registerMatchers(MatchFinder *AstMatcher) {
  ////////////////////////////////////////
  // Quick annotation conflict checkers //
  ////////////////////////////////////////

  AstMatcher->addMatcher(
      // This is a matcher on a method declaration,
      cxxMethodDecl(
          // which is marked as no dangling on temporaries,
          noDanglingOnTemporaries(),

          // and which is && ref-qualified.
          isRValueRefQualified(),

          decl().bind("invalidMethodRefQualified")),
      this);

  AstMatcher->addMatcher(
      // This is a matcher on a method declaration,
      cxxMethodDecl(
          // which is marked as no dangling on temporaries,
          noDanglingOnTemporaries(),

          // which returns a primitive type,
          returns(builtinType()),

          // and which doesn't return a pointer.
          unless(returns(pointerType())),

          decl().bind("invalidMethodPointer")),
      this);

  //////////////////
  // Main checker //
  //////////////////

  auto hasParentCall = hasParent(
      expr(anyOf(cxxOperatorCallExpr(
                     // If we're in a lamda, we may have an operator call
                     // expression ancestor in the AST, but the temporary we're
                     // matching against is not going to have the same lifetime
                     // as the constructor call.
                     unless(has(expr(ignoreTrivials(lambdaExpr())))),
                     expr().bind("parentOperatorCallExpr")),
                 callExpr(
                     // If we're in a lamda, we may have a call expression
                     // ancestor in the AST, but the temporary we're matching
                     // against is not going to have the same lifetime as the
                     // function call.
                     unless(has(expr(ignoreTrivials(lambdaExpr())))),
                     expr().bind("parentCallExpr")),
                 objcMessageExpr(
                     // If we're in a lamda, we may have an objc message
                     // expression ancestor in the AST, but the temporary we're
                     // matching against is not going to have the same lifetime
                     // as the function call.
                     unless(has(expr(ignoreTrivials(lambdaExpr())))),
                     expr().bind("parentObjCMessageExpr")),
                 cxxConstructExpr(
                     // If we're in a lamda, we may have a construct expression
                     // ancestor in the AST, but the temporary we're matching
                     // against is not going to have the same lifetime as the
                     // constructor call.
                     unless(has(expr(ignoreTrivials(lambdaExpr())))),
                     expr().bind("parentConstructExpr")))));

  AstMatcher->addMatcher(
      // This is a matcher on a method call,
      cxxMemberCallExpr(
          // which is in first party code,
          isFirstParty(),

          // and which is performed on a temporary,
          on(allOf(unless(hasType(pointerType())), isTemporary(),
                   // but which is not `this`.
                   unless(cxxThisExpr()))),

          // and which is marked as no dangling on temporaries.
          callee(cxxMethodDecl(noDanglingOnTemporaries())),

          expr().bind("memberCallExpr"),

          // We optionally match a parent call expression or a parent construct
          // expression because using a temporary inside a call is fine as long
          // as the pointer doesn't escape the function call.
          anyOf(
              // This is the case where the call is the direct parent, so we
              // know that the member call expression is the argument.
              allOf(hasParentCall, expr().bind("parentCallArg")),

              // This is the case where the call is not the direct parent, so we
              // get its child to know in which argument tree we are.
              hasAncestor(expr(hasParentCall, expr().bind("parentCallArg"))),
              // To make it optional.
              anything())),
      this);
}

void DanglingOnTemporaryChecker::check(const MatchFinder::MatchResult &Result) {
  ///////////////////////////////////////
  // Quick annotation conflict checker //
  ///////////////////////////////////////

  const char *ErrorInvalidRefQualified = "methods annotated with "
                                         "MOZ_NO_DANGLING_ON_TEMPORARIES "
                                         "cannot be && ref-qualified";

  const char *ErrorInvalidPointer = "methods annotated with "
                                    "MOZ_NO_DANGLING_ON_TEMPORARIES must "
                                    "return a pointer";

  if (auto InvalidRefQualified =
          Result.Nodes.getNodeAs<CXXMethodDecl>("invalidMethodRefQualified")) {
    diag(InvalidRefQualified->getLocation(), ErrorInvalidRefQualified,
         DiagnosticIDs::Error);
    return;
  }

  if (auto InvalidPointer =
          Result.Nodes.getNodeAs<CXXMethodDecl>("invalidMethodPointer")) {
    diag(InvalidPointer->getLocation(), ErrorInvalidPointer,
         DiagnosticIDs::Error);
    return;
  }

  //////////////////
  // Main checker //
  //////////////////

  const char *Error = "calling `%0` on a temporary, potentially allowing use "
                      "after free of the raw pointer";

  const char *EscapeStmtNote =
      "the raw pointer escapes the function scope here";

  const ObjCMessageExpr *ParentObjCMessageExpr =
      Result.Nodes.getNodeAs<ObjCMessageExpr>("parentObjCMessageExpr");

  // We don't care about cases in ObjC message expressions.
  if (ParentObjCMessageExpr) {
    return;
  }

  const CXXMemberCallExpr *MemberCall =
      Result.Nodes.getNodeAs<CXXMemberCallExpr>("memberCallExpr");

  const CallExpr *ParentCallExpr =
      Result.Nodes.getNodeAs<CallExpr>("parentCallExpr");
  const CXXConstructExpr *ParentConstructExpr =
      Result.Nodes.getNodeAs<CXXConstructExpr>("parentConstructExpr");
  const CXXOperatorCallExpr *ParentOperatorCallExpr =
      Result.Nodes.getNodeAs<CXXOperatorCallExpr>("parentOperatorCallExpr");
  const Expr *ParentCallArg = Result.Nodes.getNodeAs<Expr>("parentCallArg");

  // Just in case.
  if (!MemberCall) {
    return;
  }

  // If we have a parent call, we check whether or not we escape the function
  // being called.
  if (ParentOperatorCallExpr || ParentCallExpr || ParentConstructExpr) {
    // Just in case.
    if (!ParentCallArg) {
      return;
    }

    // No default constructor so we can't construct it using if/else.
    auto FunctionEscapeData =
        ParentOperatorCallExpr
            ? escapesFunction(ParentCallArg, ParentOperatorCallExpr)
            : ParentCallExpr
                  ? escapesFunction(ParentCallArg, ParentCallExpr)
                  : escapesFunction(ParentCallArg, ParentConstructExpr);

    // If there was an error in the escapesFunction call.
    if (std::error_code ec = FunctionEscapeData.getError()) {
      // FIXME: For now we ignore the variadic case and just consider that the
      // argument doesn't escape the function. Same for the case where we can't
      // find the function declaration or if the function is builtin.
      if (static_cast<EscapesFunctionError>(ec.value()) ==
              EscapesFunctionError::FunctionIsVariadic ||
          static_cast<EscapesFunctionError>(ec.value()) ==
              EscapesFunctionError::FunctionDeclNotFound ||
          static_cast<EscapesFunctionError>(ec.value()) ==
              EscapesFunctionError::FunctionIsBuiltin) {
        return;
      }

      // We emit the internal checker error and return.
      diag(MemberCall->getExprLoc(),
           std::string(ec.category().name()) + " error: " + ec.message(),
           DiagnosticIDs::Error);
      return;
    }

    // We deconstruct the function escape data.
    const Stmt *EscapeStmt;
    const Decl *EscapeDecl;
    std::tie(EscapeStmt, EscapeDecl) = *FunctionEscapeData;

    // If we didn't escape a parent function, we're done: we don't emit any
    // diagnostic.
    if (!EscapeStmt || !EscapeDecl) {
      return;
    }

    // We emit the error diagnostic indicating that we are calling the method
    // temporary.
    diag(MemberCall->getExprLoc(), Error, DiagnosticIDs::Error)
        << MemberCall->getMethodDecl()->getName()
        << MemberCall->getSourceRange();

    // We indicate the escape statement.
    diag(EscapeStmt->getBeginLoc(), EscapeStmtNote, DiagnosticIDs::Note)
        << EscapeStmt->getSourceRange();

    // We build the escape note along with its source range.
    StringRef EscapeDeclNote;
    SourceRange EscapeDeclRange;
    if (isa<ParmVarDecl>(EscapeDecl)) {
      EscapeDeclNote = "through the parameter declared here";
      EscapeDeclRange = EscapeDecl->getSourceRange();
    } else if (isa<VarDecl>(EscapeDecl)) {
      EscapeDeclNote = "through the variable declared here";
      EscapeDeclRange = EscapeDecl->getSourceRange();
    } else if (isa<FieldDecl>(EscapeDecl)) {
      EscapeDeclNote = "through the field declared here";
      EscapeDeclRange = EscapeDecl->getSourceRange();
    } else if (auto FuncDecl = dyn_cast<FunctionDecl>(EscapeDecl)) {
      EscapeDeclNote = "through the return value of the function declared here";
      EscapeDeclRange = FuncDecl->getReturnTypeSourceRange();
    } else {
      return;
    }

    // We emit the declaration note indicating through which decl the argument
    // escapes.
    diag(EscapeDecl->getLocation(), EscapeDeclNote, DiagnosticIDs::Note)
        << EscapeDeclRange;
  } else {
    // We emit the error diagnostic indicating that we are calling the method
    // temporary.
    diag(MemberCall->getExprLoc(), Error, DiagnosticIDs::Error)
        << MemberCall->getMethodDecl()->getName()
        << MemberCall->getSourceRange();
  }
}
