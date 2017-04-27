/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonParamInsideFunctionDeclChecker.h"
#include "CustomMatchers.h"

class NonParamAnnotation : public CustomTypeAnnotation
{
public:
  NonParamAnnotation() : CustomTypeAnnotation("moz_non_param", "non-param") {};

protected:
  // Adding alignas(_) on a struct implicitly marks it as MOZ_NON_PARAM, due to
  // MSVC limitations which prevent passing explcitly aligned types by value as
  // parameters. This overload of hasFakeAnnotation injects fake MOZ_NON_PARAM
  // annotations onto these types.
  std::string getImplicitReason(const TagDecl *D) const override {
    // Check if the decl itself has an AlignedAttr on it.
    for (const Attr *A : D->attrs()) {
      if (isa<AlignedAttr>(A)) {
        return "it has an alignas(_) annotation";
      }
    }

    // Check if any of the decl's fields have an AlignedAttr on them.
    if (auto RD = dyn_cast<RecordDecl>(D)) {
      for (auto F : RD->fields()) {
        for (auto A : F->attrs()) {
          if (isa<AlignedAttr>(A)) {
            return ("member '" + F->getName() + "' has an alignas(_) annotation").str();
          }
        }
      }
    }

    // We don't need to check the types of fields, as the CustomTypeAnnotation
    // infrastructure will handle that for us.
    return "";
  }
};
NonParamAnnotation NonParam;

void NonParamInsideFunctionDeclChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      functionDecl(anyOf(allOf(isDefinition(),
                               hasAncestor(classTemplateSpecializationDecl()
                                               .bind("spec"))),
                         isDefinition()))
          .bind("func"),
      this);
  AstMatcher->addMatcher(
      lambdaExpr().bind("lambda"),
      this);
}

void NonParamInsideFunctionDeclChecker::check(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const FunctionDecl*> CheckedFunctionDecls;

  const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  if (!func) {
    const LambdaExpr *lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda");
    if (lambda) {
      func = lambda->getCallOperator();
    }
  }

  if (!func) {
    return;
  }

  if (func->isDeleted()) {
    return;
  }

  // We need to skip decls which have these types as parameters in system
  // headers, because presumably those headers act like an assertion that the
  // alignment will be preserved in that situation.
  if (getDeclarationNamespace(func) == "std") {
    return;
  }

  if (inThirdPartyPath(func)) {
    return;
  }

  // Don't report errors on the same declarations more than once.
  if (CheckedFunctionDecls.count(func)) {
    return;
  }
  CheckedFunctionDecls.insert(func);

  const ClassTemplateSpecializationDecl *Spec =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("spec");

  for (ParmVarDecl *p : func->parameters()) {
    QualType T = p->getType().withoutLocalFastQualifiers();
    if (NonParam.hasEffectiveAnnotation(T)) {
      diag(p->getLocation(), "Type %0 must not be used as parameter",
           DiagnosticIDs::Error) << T;
      diag(p->getLocation(), "Please consider passing a const reference instead",
           DiagnosticIDs::Note);

      if (Spec) {
        diag(Spec->getPointOfInstantiation(),
             "The bad argument was passed to %0 here",
             DiagnosticIDs::Note)
          << Spec->getSpecializedTemplate();
      }

      NonParam.dumpAnnotationReason(*this, T, p->getLocation());
    }
  }
}
