/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonStdMoveChecker.h"
#include "CustomMatchers.h"
#include "clang/Lex/Lexer.h"

constexpr const char *kConstructExpr = "construct";
constexpr const char *kOperatorCallExpr = "operator-call";
constexpr const char *kSourceExpr = "source-expr";
constexpr const char *kMaterializeExpr = "materialize-expr";

void NonStdMoveChecker::registerMatchers(MatchFinder *AstMatcher) {

  // Assignment through forget
  AstMatcher->addMatcher(
      cxxOperatorCallExpr(
          hasOverloadedOperatorName("="),
          hasAnyArgument(materializeTemporaryExpr(
                             has(cxxBindTemporaryExpr(has(cxxMemberCallExpr(
                                 has(memberExpr(member(hasName("forget")))),
                                 on(expr().bind(kSourceExpr)))))))
                             .bind(kMaterializeExpr)))
          .bind(kOperatorCallExpr),
      this);

  // Construction through forget

  AstMatcher->addMatcher(
      cxxConstructExpr(has(materializeTemporaryExpr(
                               has(cxxBindTemporaryExpr(has(cxxMemberCallExpr(
                                   has(memberExpr(member(hasName("forget")))),
                                   on(expr().bind(kSourceExpr)))))))
                               .bind(kMaterializeExpr)))
          .bind(kConstructExpr),
      this);
}

Optional<FixItHint>
NonStdMoveChecker::makeFixItHint(const MatchFinder::MatchResult &Result,
                                 const Expr *const TargetExpr) {
  const auto *MaterializeExpr = Result.Nodes.getNodeAs<Expr>(kMaterializeExpr);

  // TODO: In principle, we should check here if TargetExpr if
  // assignable/constructible from std::move(SourceExpr). Not sure how to do
  // this. Currently, we only filter out the case where the targetTypeTemplate
  // is already_AddRefed, where this is known to fail.

  const auto *targetTypeTemplate = getNonTemplateSpecializedCXXRecordDecl(
      TargetExpr->getType().getCanonicalType());
  const auto *sourceTypeTemplate = getNonTemplateSpecializedCXXRecordDecl(
      MaterializeExpr->getType().getCanonicalType());

  if (targetTypeTemplate && sourceTypeTemplate) {
    // TODO is there a better way to check this than by name? otherwise, the
    // names probably are necessarily unique in the scope
    if (targetTypeTemplate->getName() == sourceTypeTemplate->getName() &&
        targetTypeTemplate->getName() == "already_AddRefed") {
      return {};
    }
  }

  const auto *SourceExpr = Result.Nodes.getNodeAs<Expr>(kSourceExpr);

  const auto sourceText = Lexer::getSourceText(
      CharSourceRange::getTokenRange(SourceExpr->getSourceRange()),
      Result.Context->getSourceManager(), Result.Context->getLangOpts());

  return FixItHint::CreateReplacement(MaterializeExpr->getSourceRange(),
                                      ("std::move(" + sourceText + ")").str());
}

void NonStdMoveChecker::check(const MatchFinder::MatchResult &Result) {
  // TODO: Include source and target type name in messages.

  const auto *OCE =
      Result.Nodes.getNodeAs<CXXOperatorCallExpr>(kOperatorCallExpr);

  if (OCE) {
    const auto *refPtrDecl =
        dyn_cast<const CXXRecordDecl>(OCE->getCalleeDecl()->getDeclContext());

    const auto XFixItHint = makeFixItHint(Result, OCE);
    // TODO: produce diagnostic but no FixItHint in this case?
    if (XFixItHint) {
      diag(OCE->getBeginLoc(), "non-standard move assignment to %0 obscures "
                               "move, use std::move instead")
          << refPtrDecl << *XFixItHint;
    }
  }

  const auto *CoE = Result.Nodes.getNodeAs<CXXConstructExpr>(kConstructExpr);

  if (CoE) {
    const auto *refPtrDecl =
        dyn_cast<const CXXRecordDecl>(CoE->getConstructor()->getDeclContext());

    const auto XFixItHint = makeFixItHint(Result, CoE);
    // TODO: produce diagnostic but no FixItHint in this case?
    if (XFixItHint) {
      diag(CoE->getBeginLoc(), "non-standard move construction of %0 obscures "
                               "move, use std::move instead")
          << refPtrDecl << *XFixItHint;
    }
  }

  // TODO: What about swap calls immediately after default-construction? These
  // can also be replaced by move-construction, but this may require
  // control-flow analysis.
}
