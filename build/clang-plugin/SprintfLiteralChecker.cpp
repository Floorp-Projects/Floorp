/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SprintfLiteralChecker.h"
#include "CustomMatchers.h"

void SprintfLiteralChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      callExpr(isSnprintfLikeFunc(),
        allOf(hasArgument(0, ignoringParenImpCasts(declRefExpr().bind("buffer"))),
                             anyOf(hasArgument(1, sizeOfExpr(hasIgnoringParenImpCasts(declRefExpr().bind("size")))),
                                   hasArgument(1, integerLiteral().bind("immediate")),
                                   hasArgument(1, declRefExpr(to(varDecl(hasType(isConstQualified()),
                                                                         hasInitializer(integerLiteral().bind("constant")))))))))
        .bind("funcCall"),
      this
  );
}

void SprintfLiteralChecker::check(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // SprintfLiteral is not usable in C, so there is no point in issuing these
    // warnings.
    return;
  }

  const char* Error =
    "Use %1 instead of %0 when writing into a character array.";
  const char* Note =
    "This will prevent passing in the wrong size to %0 accidentally.";

  const CallExpr *D = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  StringRef Name = D->getDirectCallee()->getName();
  const char *Replacement;
  if (Name == "snprintf") {
    Replacement = "SprintfLiteral";
  } else {
    assert(Name == "vsnprintf");
    Replacement = "VsprintfLiteral";
  }

  const DeclRefExpr *Buffer = Result.Nodes.getNodeAs<DeclRefExpr>("buffer");
  const DeclRefExpr *Size = Result.Nodes.getNodeAs<DeclRefExpr>("size");
  if (Size) {
    // Match calls like snprintf(x, sizeof(x), ...).
    if (Buffer->getFoundDecl() != Size->getFoundDecl()) {
      return;
    }

    diag(D->getLocStart(), Error, DiagnosticIDs::Error) << Name << Replacement;
    diag(D->getLocStart(), Note, DiagnosticIDs::Note) << Name;
    return;
  }

  const QualType QType = Buffer->getType();
  const ConstantArrayType *Type = dyn_cast<ConstantArrayType>(QType.getTypePtrOrNull());
  if (Type) {
    // Match calls like snprintf(x, 100, ...), where x is int[100];
    const IntegerLiteral *Literal = Result.Nodes.getNodeAs<IntegerLiteral>("immediate");
    if (!Literal) {
      // Match calls like: const int y = 100; snprintf(x, y, ...);
      Literal = Result.Nodes.getNodeAs<IntegerLiteral>("constant");
    }

    if (Type->getSize().ule(Literal->getValue())) {
      diag(D->getLocStart(), Error, DiagnosticIDs::Error) << Name << Replacement;
      diag(D->getLocStart(), Note, DiagnosticIDs::Note) << Name;
    }
  }
}
