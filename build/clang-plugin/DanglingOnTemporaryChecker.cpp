/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DanglingOnTemporaryChecker.h"
#include "CustomMatchers.h"

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

          // and which doesn't return a pointer.
          unless(returns(pointerType())),

          decl().bind("invalidMethodPointer")),
      this);

  //////////////////
  // Main checker //
  //////////////////

  AstMatcher->addMatcher(
      // This is a matcher on a method call,
      cxxMemberCallExpr(
          // which is performed on a temporary,
          onImplicitObjectArgument(materializeTemporaryExpr()),

          // and which is marked as no dangling on temporaries.
          callee(cxxMethodDecl(noDanglingOnTemporaries())),

          anyOf(
              // We care only about the cases where the method call is NOT an
              // argument in a call expression. If it is in a call expression,
              // the temporary lives long enough so that it's valid to use the
              // pointer.
              unless(hasNonTrivialParent(callExpr())),
              // Unless the argument somehow escapes the function scope through
              // globals/statics/black magic.
              escapesParentFunctionCall(
                  stmt().bind("escapeStatement"),
                  decl().bind("escapeDeclaration"))),

          expr().bind("memberCallExpr")),
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

  if (auto InvalidRefQualified
        = Result.Nodes.getNodeAs<CXXMethodDecl>("invalidMethodRefQualified")) {
    diag(InvalidRefQualified->getLocation(), ErrorInvalidRefQualified,
         DiagnosticIDs::Error);
    return;
  }

  if (auto InvalidPointer
        = Result.Nodes.getNodeAs<CXXMethodDecl>("invalidMethodPointer")) {
    diag(InvalidPointer->getLocation(), ErrorInvalidPointer,
         DiagnosticIDs::Error);
    return;
  }

  //////////////////
  // Main checker //
  //////////////////

  const char *Error = "calling `%0` on a temporary, potentially allowing use "
                      "after free of the raw pointer";

  const char *EscapeStmtNote
      = "the raw pointer escapes the function scope here";

  const CXXMemberCallExpr *MemberCall =
      Result.Nodes.getNodeAs<CXXMemberCallExpr>("memberCallExpr");

  // If we escaped the a parent function call, we get the statement and the
  // associated declaration.
  const Stmt *EscapeStmt =
      Result.Nodes.getNodeAs<Stmt>("escapeStatement");
  const Decl *EscapeDecl =
      Result.Nodes.getNodeAs<Decl>("escapeDeclaration");

  // Just in case.
  if (!MemberCall) {
    return;
  }

  // We emit the error diagnostic indicating that we are calling the method
  // temporary.
  diag(MemberCall->getExprLoc(), Error, DiagnosticIDs::Error)
      << MemberCall->getMethodDecl()->getName()
      << MemberCall->getSourceRange();

  // If we didn't escape a parent function, we're done.
  if (!EscapeStmt || !EscapeDecl) {
    return;
  }

  diag(EscapeStmt->getLocStart(), EscapeStmtNote, DiagnosticIDs::Note)
      << EscapeStmt->getSourceRange();

  StringRef EscapeDeclNote;
  SourceRange EscapeDeclRange;
  if (isa<ParmVarDecl>(EscapeDecl)) {
    EscapeDeclNote = "through the parameter declared here";
    EscapeDeclRange = EscapeDecl->getSourceRange();
  } else if (isa<VarDecl>(EscapeDecl)) {
    EscapeDeclNote = "through the variable declared here";
    EscapeDeclRange = EscapeDecl->getSourceRange();
  } else if (auto FuncDecl = dyn_cast<FunctionDecl>(EscapeDecl)) {
    EscapeDeclNote = "through the return value of the function declared here";
    EscapeDeclRange = FuncDecl->getReturnTypeSourceRange();
  } else {
    return;
  }

  diag(EscapeDecl->getLocation(), EscapeDeclNote, DiagnosticIDs::Note)
      << EscapeDeclRange;
}
