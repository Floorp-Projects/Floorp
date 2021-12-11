/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonTrivialTypeInFfiChecker.h"
#include "CustomMatchers.h"

void NonTrivialTypeInFfiChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(functionDecl(isExternC()).bind("func"), this);
}

void NonTrivialTypeInFfiChecker::check(const MatchFinder::MatchResult &Result) {
  static DenseSet<const FunctionDecl *> CheckedFunctionDecls;

  const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  // Don't report errors on the same declarations more than once.
  if (!CheckedFunctionDecls.insert(func).second) {
    return;
  }

  if (inThirdPartyPath(func)) {
    return;
  }

  auto NoteFor = [](const QualType &T) -> std::string {
    std::string s = "Please consider using a pointer or reference";
    if (T->getAs<TemplateSpecializationType>()) {
      s += ", or explicitly instantiating the template";
    }
    return s + " instead";
  };

  for (ParmVarDecl *p : func->parameters()) {
    QualType T = p->getType().getUnqualifiedType();
    if (!T->isVoidType() && !T->isReferenceType() &&
        !T.isTriviallyCopyableType(*Result.Context)) {
      diag(p->getLocation(),
           "Type %0 must not be used as parameter to extern "
           "\"C\" function",
           DiagnosticIDs::Error)
          << T;
      diag(p->getLocation(), NoteFor(T), DiagnosticIDs::Note);
    }
  }

  QualType T = func->getReturnType().getUnqualifiedType();
  if (!T->isVoidType() && !T->isReferenceType() &&
      !T.isTriviallyCopyableType(*Result.Context)) {
    diag(func->getLocation(),
         "Type %0 must not be used as return type of "
         "extern \"C\" function",
         DiagnosticIDs::Error)
        << T;
    diag(func->getLocation(), NoteFor(T), DiagnosticIDs::Note);
  }
}
