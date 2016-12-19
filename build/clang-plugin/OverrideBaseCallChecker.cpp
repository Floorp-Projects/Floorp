/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OverrideBaseCallChecker.h"
#include "CustomMatchers.h"

void OverrideBaseCallChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(cxxRecordDecl(hasBaseClasses()).bind("class"),
      this);
}

bool OverrideBaseCallChecker::isRequiredBaseMethod(
    const CXXMethodDecl *Method) {
  return hasCustomAnnotation(Method, "moz_required_base_method");
}

void OverrideBaseCallChecker::evaluateExpression(
    const Stmt *StmtExpr, std::list<const CXXMethodDecl*> &MethodList) {
  // Continue while we have methods in our list
  if (!MethodList.size()) {
    return;
  }

  if (auto MemberFuncCall = dyn_cast<CXXMemberCallExpr>(StmtExpr)) {
    if (auto Method = dyn_cast<CXXMethodDecl>(
        MemberFuncCall->getDirectCallee())) {
      findBaseMethodCall(Method, MethodList);
    }
  }

  for (auto S : StmtExpr->children()) {
    if (S) {
      evaluateExpression(S, MethodList);
    }
  }
}

void OverrideBaseCallChecker::getRequiredBaseMethod(
    const CXXMethodDecl *Method,
    std::list<const CXXMethodDecl*>& MethodsList) {

  if (isRequiredBaseMethod(Method)) {
    MethodsList.push_back(Method);
  } else {
    // Loop through all it's base methods.
    for (auto BaseMethod = Method->begin_overridden_methods();
        BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
      getRequiredBaseMethod(*BaseMethod, MethodsList);
    }
  }
}

void OverrideBaseCallChecker::findBaseMethodCall(
    const CXXMethodDecl* Method,
    std::list<const CXXMethodDecl*>& MethodsList) {

  MethodsList.remove(Method);
  // Loop also through all it's base methods;
  for (auto BaseMethod = Method->begin_overridden_methods();
      BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
    findBaseMethodCall(*BaseMethod, MethodsList);
  }
}

void OverrideBaseCallChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error =
      "Method %0 must be called in all overrides, but is not called in "
      "this override defined for class %1";
  const CXXRecordDecl *Decl = Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  // Loop through the methods and look for the ones that are overridden.
  for (auto Method : Decl->methods()) {
    // If this method doesn't override other methods or it doesn't have a body,
    // continue to the next declaration.
    if (!Method->size_overridden_methods() || !Method->hasBody()) {
      continue;
    }

    // Preferred the usage of list instead of vector in order to avoid
    // calling erase-remove when deleting items
    std::list<const CXXMethodDecl*> MethodsList;
    // For each overridden method push it to a list if it meets our
    // criteria
    for (auto BaseMethod = Method->begin_overridden_methods();
        BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
      getRequiredBaseMethod(*BaseMethod, MethodsList);
    }

    // If no method has been found then no annotation was used
    // so checking is not needed
    if (!MethodsList.size()) {
      continue;
    }

    // Loop through the body of our method and search for calls to
    // base methods
    evaluateExpression(Method->getBody(), MethodsList);

    // If list is not empty pop up errors
    for (auto BaseMethod : MethodsList) {
      diag(Method->getLocation(), Error, DiagnosticIDs::Error)
          << BaseMethod->getQualifiedNameAsString()
          << Decl->getName();
    }
  }
}
