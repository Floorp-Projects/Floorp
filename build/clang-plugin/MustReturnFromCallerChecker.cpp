/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MustReturnFromCallerChecker.h"
#include "CustomMatchers.h"

void MustReturnFromCallerChecker::registerMatchers(MatchFinder *AstMatcher) {
  // Look for a call to a MOZ_MUST_RETURN_FROM_CALLER function
  AstMatcher->addMatcher(
      callExpr(callee(functionDecl(isMozMustReturnFromCaller())),
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
  assert(Block && "This statement should be within the CFG!");

  if (!immediatelyReturns(Block, Result.Context, CallIndex + 1)) {
    diag(Call->getLocStart(),
         "You must immediately return after calling this function",
         DiagnosticIDs::Error);
  }
}

bool MustReturnFromCallerChecker::immediatelyReturns(
    RecurseGuard<const CFGBlock *> Block, ASTContext *TheContext,
    size_t FromIdx) {
  if (Block.isRepeat()) {
    return false;
  }

  for (size_t I = FromIdx; I < Block->size(); ++I) {
    Optional<CFGStmt> S = (*Block)[I].getAs<CFGStmt>();
    if (!S) {
      continue;
    }

    auto AfterTrivials = IgnoreTrivials(S->getStmt());

    // If we are looking at a ConstructExpr, a DeclRefExpr or a MemberExpr it's
    // OK to use them after a call to a MOZ_MUST_RETURN_FROM_CALLER function.
    // It is also, of course, OK to look at a ReturnStmt.
    if (isa<ReturnStmt>(AfterTrivials) ||
        isa<CXXConstructExpr>(AfterTrivials) ||
        isa<DeclRefExpr>(AfterTrivials) || isa<MemberExpr>(AfterTrivials)) {
      continue;
    }

    // It's also OK to call any function or method which is annotated with
    // MOZ_MAY_CALL_AFTER_MUST_RETURN. We consider all CXXConversionDecls
    // to be MOZ_MAY_CALL_AFTER_MUST_RETURN (like operator T*()).
    if (auto CE = dyn_cast<CallExpr>(AfterTrivials)) {
      auto Callee = CE->getDirectCallee();
      if (Callee &&
          hasCustomAnnotation(Callee, "moz_may_call_after_must_return")) {
        continue;
      }

      if (Callee && isa<CXXConversionDecl>(Callee)) {
        continue;
      }
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
