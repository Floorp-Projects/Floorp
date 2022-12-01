/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonParamInsideFunctionDeclChecker.h"
#include "CustomMatchers.h"
#include "clang/Basic/TargetInfo.h"

class NonParamAnnotation : public CustomTypeAnnotation {
public:
  NonParamAnnotation() : CustomTypeAnnotation(moz_non_param, "non-param"){};

protected:
  // Helper for checking if a Decl has an explicitly specified alignment.
  // Returns the alignment, in char units, of the largest alignment attribute,
  // if it exceeds pointer alignment, and 0 otherwise.
  static unsigned checkExplicitAlignment(const Decl *D) {
    ASTContext &Context = D->getASTContext();
#if CLANG_VERSION_FULL >= 1600
    unsigned PointerAlign = Context.getTargetInfo().getPointerAlign(LangAS::Default);
#else
    unsigned PointerAlign = Context.getTargetInfo().getPointerAlign(0);
#endif

    // getMaxAlignment gets the largest alignment, in bits, specified by an
    // alignment attribute directly on the declaration. If no alignment
    // attribute is specified, it will return `0`.
    unsigned MaxAlign = D->getMaxAlignment();
    if (MaxAlign > PointerAlign) {
      return Context.toCharUnitsFromBits(MaxAlign).getQuantity();
    }
    return 0;
  }

  // This is directly derived from the logic in Clang's `canPassInRegisters`
  // function, from `SemaDeclCXX`. It is used instead of `canPassInRegisters` to
  // behave consistently on 64-bit windows platforms which are overly
  // permissive, allowing too many types to be passed in registers.
  //
  // Types which can be passed in registers will be re-aligned in the called
  // function by clang, so aren't impacted by the win32 object passing ABI
  // alignment issue.
  static bool canPassAsTemporary(const CXXRecordDecl *D) {
    // Per C++ [class.temporary]p3:
    //
    // When an object of class type X is passed to or returned from a function,
    // if X has at least one eligible copy or move constructor ([special]), each
    // such constructor is trivial, and the destructor of X is either trivial or
    // deleted, implementations are permitted to create a temporary object to
    // hold the function parameter or result object.
    //
    // The temporary object is constructed from the function argument or return
    // value, respectively, and the function's parameter or return object is
    // initialized as if by using the eligible trivial constructor to copy the
    // temporary (even if that constructor is inaccessible or would not be
    // selected by overload resolution to perform a copy or move of the object).
    bool HasNonDeletedCopyOrMove = false;

    if (D->needsImplicitCopyConstructor() &&
        !D->defaultedCopyConstructorIsDeleted()) {
      if (!D->hasTrivialCopyConstructorForCall())
        return false;
      HasNonDeletedCopyOrMove = true;
    }

    if (D->needsImplicitMoveConstructor() &&
        !D->defaultedMoveConstructorIsDeleted()) {
      if (!D->hasTrivialMoveConstructorForCall())
        return false;
      HasNonDeletedCopyOrMove = true;
    }

    if (D->needsImplicitDestructor() && !D->defaultedDestructorIsDeleted() &&
        !D->hasTrivialDestructorForCall())
      return false;

    for (const CXXMethodDecl *MD : D->methods()) {
      if (MD->isDeleted())
        continue;

      auto *CD = dyn_cast<CXXConstructorDecl>(MD);
      if (CD && CD->isCopyOrMoveConstructor())
        HasNonDeletedCopyOrMove = true;
      else if (!isa<CXXDestructorDecl>(MD))
        continue;

      if (!MD->isTrivialForCall())
        return false;
    }

    return HasNonDeletedCopyOrMove;
  }

  // Adding alignas(_) on a struct implicitly marks it as MOZ_NON_PARAM, due to
  // MSVC limitations which prevent passing explcitly aligned types by value as
  // parameters. This overload of hasFakeAnnotation injects fake MOZ_NON_PARAM
  // annotations onto these types.
  std::string getImplicitReason(const TagDecl *D,
                                VisitFlags &ToVisit) const override {
    // Some stdlib types are known to have alignments over the pointer size on
    // non-win32 platforms, but should not be linted against. Clear any
    // annotations on those types.
    if (!D->getASTContext().getTargetInfo().getCXXABI().isMicrosoft() &&
        getDeclarationNamespace(D) == "std") {
      StringRef Name = getNameChecked(D);
      if (Name == "function") {
        ToVisit = VISIT_NONE;
        return "";
      }
    }

    // If the type doesn't have a destructor and can be passed with a temporary,
    // clang will handle re-aligning it for us automatically, and we don't need
    // to worry about the passed alignment.
    auto RD = dyn_cast<CXXRecordDecl>(D);
    if (RD && RD->isCompleteDefinition() && canPassAsTemporary(RD)) {
      return "";
    }

    // Check if the decl itself has an explicit alignment on it.
    if (unsigned ExplicitAlign = checkExplicitAlignment(D)) {
      return "it has an explicit alignment of '" +
             std::to_string(ExplicitAlign) + "'";
    }

    // Check if any of the decl's fields have an explicit alignment on them.
    if (auto RD = dyn_cast<RecordDecl>(D)) {
      for (auto F : RD->fields()) {
        if (unsigned ExplicitAlign = checkExplicitAlignment(F)) {
          return ("member '" + F->getName() +
                  "' has an explicit alignment of '" +
                  std::to_string(ExplicitAlign) + "'")
              .str();
        }
      }
    }

    // We don't need to check the types of fields, as the CustomTypeAnnotation
    // infrastructure will handle that for us.
    return "";
  }
};
NonParamAnnotation NonParam;

void NonParamInsideFunctionDeclChecker::registerMatchers(
    MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      functionDecl(
          anyOf(allOf(isDefinition(),
                      hasAncestor(
                          classTemplateSpecializationDecl().bind("spec"))),
                isDefinition()))
          .bind("func"),
      this);
  AstMatcher->addMatcher(lambdaExpr().bind("lambda"), this);
}

void NonParamInsideFunctionDeclChecker::check(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const FunctionDecl *> CheckedFunctionDecls;

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
           DiagnosticIDs::Error)
          << T;
      diag(p->getLocation(),
           "Please consider passing a const reference instead",
           DiagnosticIDs::Note);

      if (Spec) {
        diag(Spec->getPointOfInstantiation(),
             "The bad argument was passed to %0 here", DiagnosticIDs::Note)
            << Spec->getSpecializedTemplate();
      }

      NonParam.dumpAnnotationReason(*this, T, p->getLocation());
    }
  }
}
