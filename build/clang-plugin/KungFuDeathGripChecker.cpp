/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KungFuDeathGripChecker.h"
#include "CustomMatchers.h"

void KungFuDeathGripChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(varDecl(allOf(hasType(isRefPtr()), hasLocalStorage(),
                                       hasInitializer(anything())))
                             .bind("decl"),
                         this);
}

void KungFuDeathGripChecker::check(const MatchFinder::MatchResult &Result) {
  const char *Error = "Unused \"kungFuDeathGrip\" %0 objects constructed from "
                      "%1 are prohibited";
  const char *Note = "Please switch all accesses to this %0 to go through "
                     "'%1', or explicitly pass '%1' to `mozilla::Unused`";

  const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("decl");
  if (D->isReferenced()) {
    return;
  }

  // Not interested in parameters.
  if (isa<ImplicitParamDecl>(D) || isa<ParmVarDecl>(D)) {
    return;
  }

  const Expr *E = IgnoreTrivials(D->getInit());
  const CXXConstructExpr *CE = dyn_cast<CXXConstructExpr>(E);
  if (CE && CE->getNumArgs() == 0) {
    // We don't report an error when we construct and don't use a nsCOMPtr /
    // nsRefPtr with no arguments. We don't report it because the error is not
    // related to the current check. In the future it may be reported through a
    // more generic mechanism.
    return;
  }

  // We don't want to look at the single argument conversion constructors
  // which are inbetween the declaration and the actual object which we are
  // assigning into the nsCOMPtr/RefPtr. To do this, we repeatedly
  // IgnoreTrivials, then look at the expression. If it is one of these
  // conversion constructors, we ignore it and continue to dig.
  while ((CE = dyn_cast<CXXConstructExpr>(E)) && CE->getNumArgs() == 1) {
    E = IgnoreTrivials(CE->getArg(0));
  }

  // If the argument expression is an xvalue, we are not taking a copy of
  // anything.
  if (E->isXValue()) {
    return;
  }

  // It is possible that the QualType doesn't point to a type yet so we are
  // not interested.
  if (E->getType().isNull()) {
    return;
  }

  // We allow taking a kungFuDeathGrip of `this` because it cannot change
  // beneath us, so calling directly through `this` is OK. This is the same
  // for local variable declarations.
  //
  // We also don't complain about unused RefPtrs which are constructed from
  // the return value of a new expression, as these are required in order to
  // immediately destroy the value created (which was presumably created for
  // its side effects), and are not used as a death grip.
  if (isa<CXXThisExpr>(E) || isa<DeclRefExpr>(E) || isa<CXXNewExpr>(E)) {
    return;
  }

  // These types are assigned into nsCOMPtr and RefPtr for their side effects,
  // and not as a kungFuDeathGrip. We don't want to consider RefPtr and nsCOMPtr
  // types which are initialized with these types as errors.
  const TagDecl *TD = E->getType()->getAsTagDecl();
  if (TD && TD->getIdentifier()) {
    static const char *IgnoreTypes[] = {
        "already_AddRefed",
        "nsGetServiceByCID",
        "nsGetServiceByCIDWithError",
        "nsGetServiceByContractID",
        "nsGetServiceByContractIDWithError",
        "nsCreateInstanceByCID",
        "nsCreateInstanceByContractID",
        "nsCreateInstanceFromFactory",
    };

    for (uint32_t i = 0; i < sizeof(IgnoreTypes) / sizeof(IgnoreTypes[0]);
         ++i) {
      if (TD->getName() == IgnoreTypes[i]) {
        return;
      }
    }
  }

  // Report the error
  const char *ErrThing;
  const char *NoteThing;
  if (isa<MemberExpr>(E)) {
    ErrThing = "members";
    NoteThing = "member";
  } else {
    ErrThing = "temporary values";
    NoteThing = "value";
  }

  // We cannot provide the note if we don't have an initializer
  diag(D->getBeginLoc(), Error, DiagnosticIDs::Error)
      << D->getType() << ErrThing;
  diag(E->getBeginLoc(), Note, DiagnosticIDs::Note)
      << NoteThing << getNameChecked(D);
}
