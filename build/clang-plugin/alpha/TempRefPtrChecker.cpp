/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TempRefPtrChecker.h"
#include "CustomMatchers.h"

constexpr const char *kCallExpr = "call-expr";
constexpr const char *kOperatorCallExpr = "operator-call";

void TempRefPtrChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      cxxOperatorCallExpr(
          hasOverloadedOperatorName("->"),
          hasAnyArgument(implicitCastExpr(
              hasSourceExpression(materializeTemporaryExpr(anyOf(
                  hasDescendant(callExpr().bind(kCallExpr)), anything()))))),
          callee(hasDeclContext(classTemplateSpecializationDecl(
              isSmartPtrToRefCountedDecl(),
              // ignore any calls on temporary RefPtr<MozPromise<T>>,
              // since these typically need to be locally ref-counted,
              // e.g. in Then chains where the promise might be resolved
              // concurrently
              unless(hasTemplateArgument(
                  0, refersToType(hasDeclaration(
                         cxxRecordDecl(hasName("mozilla::MozPromise"))))))))))
          .bind(kOperatorCallExpr),
      this);
}

void TempRefPtrChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *OCE =
      Result.Nodes.getNodeAs<CXXOperatorCallExpr>(kOperatorCallExpr);

  const auto *refPtrDecl =
      dyn_cast<const CXXRecordDecl>(OCE->getCalleeDecl()->getDeclContext());

  diag(OCE->getOperatorLoc(),
       "performance issue: temporary %0 is only dereferenced here once which "
       "involves short-lived AddRef/Release calls")
      << refPtrDecl;

  const auto *InnerCE = Result.Nodes.getNodeAs<CallExpr>(kCallExpr);
  if (InnerCE) {
    const auto functionName =
        InnerCE->getCalleeDecl()->getAsFunction()->getQualifiedNameAsString();

    if (functionName != "mozilla::MakeRefPtr") {
      diag(
          OCE->getOperatorLoc(),
          "consider changing function %0 to return a raw reference instead (be "
          "sure that the pointee is held alive by someone else though!)",
          DiagnosticIDs::Note)
          << functionName;
    }
  }
}
