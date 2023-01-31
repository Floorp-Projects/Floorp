/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MustReturnFromCallerChecker.h"
#include "CustomMatchers.h"

void MustReturnFromCallerChecker::registerMatchers(MatchFinder *AstMatcher) {
  // Look for a call to a MOZ_MUST_RETURN_FROM_CALLER member
  AstMatcher->addMatcher(
      cxxMemberCallExpr(
          on(declRefExpr(to(parmVarDecl()))),
          callee(functionDecl(isMozMustReturnFromCaller())),
          anyOf(hasAncestor(lambdaExpr().bind("containing-lambda")),
                hasAncestor(functionDecl().bind("containing-func"))))
          .bind("call"),
      this);
}

void MustReturnFromCallerChecker::check(
    const MatchFinder::MatchResult &Result) {
  const auto *ContainingLambda =
      Result.Nodes.getNodeAs<LambdaExpr>("containing-lambda");
  const auto *ContainingFunc =
      Result.Nodes.getNodeAs<FunctionDecl>("containing-func");
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");

  Stmt *Body = nullptr;
  if (ContainingLambda) {
    Body = ContainingLambda->getBody();
  } else if (ContainingFunc) {
    Body = ContainingFunc->getBody();
  } else {
    return;
  }
  assert(Body && "Should have a body by this point");

  // Generate the CFG for the enclosing function or decl.
  CFG::BuildOptions Options;
  std::unique_ptr<CFG> TheCFG =
      CFG::buildCFG(nullptr, Body, Result.Context, Options);
  if (!TheCFG) {
    return;
  }

  // Determine which block in the CFG we want to look at the successors of.
  StmtToBlockMap BlockMap(TheCFG.get(), Result.Context);
  size_t CallIndex;
  const auto *Block = BlockMap.blockContainingStmt(Call, &CallIndex);
  if (!Block) {
    // This statement is not within the CFG!
    return;
  }

  if (!immediatelyReturns(Block, Result.Context, CallIndex + 1)) {
    diag(Call->getBeginLoc(),
         "You must immediately return after calling this function",
         DiagnosticIDs::Error);
  }
}

bool MustReturnFromCallerChecker::isIgnorable(const Stmt *S) {
  auto AfterTrivials = IgnoreTrivials(S);

  // After a call to MOZ_MUST_RETURN_FROM_CALLER function it's ok to have any of
  // these expressions.
  if (isa<ReturnStmt>(AfterTrivials) || isa<CXXConstructExpr>(AfterTrivials) ||
      isa<DeclRefExpr>(AfterTrivials) || isa<MemberExpr>(AfterTrivials) ||
      isa<IntegerLiteral>(AfterTrivials) ||
      isa<FloatingLiteral>(AfterTrivials) ||
      isa<CXXNullPtrLiteralExpr>(AfterTrivials) ||
      isa<CXXBoolLiteralExpr>(AfterTrivials)) {
    return true;
  }

  // Solitary `this` should be permited, like in the context `return this;`
  if (auto TE = dyn_cast<CXXThisExpr>(AfterTrivials)) {
    if (TE->child_begin() == TE->child_end()) {
      return true;
    }
    return false;
  }

  // For UnaryOperator make sure we only accept arithmetic operations.
  if (auto UO = dyn_cast<UnaryOperator>(AfterTrivials)) {
    if (!UO->isArithmeticOp()) {
      return false;
    }
    return isIgnorable(UO->getSubExpr());
  }

  // It's also OK to call any function or method which is annotated with
  // MOZ_MAY_CALL_AFTER_MUST_RETURN. We consider all CXXConversionDecls
  // to be MOZ_MAY_CALL_AFTER_MUST_RETURN (like operator T*()).
  if (auto CE = dyn_cast<CallExpr>(AfterTrivials)) {
    auto Callee = CE->getDirectCallee();
    if (Callee && hasCustomAttribute<moz_may_call_after_must_return>(Callee)) {
      return true;
    }

    if (Callee && isa<CXXConversionDecl>(Callee)) {
      return true;
    }
  }
  return false;
}

bool MustReturnFromCallerChecker::immediatelyReturns(
    RecurseGuard<const CFGBlock *> Block, ASTContext *TheContext,
    size_t FromIdx) {
  if (Block.isRepeat()) {
    return false;
  }

  for (size_t I = FromIdx; I < Block->size(); ++I) {
    auto S = (*Block)[I].getAs<CFGStmt>();
    if (!S) {
      continue;
    }

    // Some statements should be ignored by default due to their CFG context.
    if (isIgnorable(S->getStmt())) {
      continue;
    }

    // Otherwise, this expression is problematic.
    return false;
  }

  for (auto Succ = Block->succ_begin(); Succ != Block->succ_end(); ++Succ) {
    if (!immediatelyReturns(Block.recurse(*Succ), TheContext, 0)) {
      return false;
    }
  }
  return true;
}
