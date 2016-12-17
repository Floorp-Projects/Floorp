/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DiagnosticsMatcher.h"

DiagnosticsMatcher::DiagnosticsMatcher() {
  Scope.registerMatcher(AstMatcher);
  ArithmeticArg.registerMatcher(AstMatcher);
  TrivialCtorDtor.registerMatcher(AstMatcher);
  NaNExpr.registerMatcher(AstMatcher);
  NoAddRefReleaseOnReturn.registerMatcher(AstMatcher);
  RefCountedInsideLambda.registerMatcher(AstMatcher);
  ExplicitOperatorBool.registerMatcher(AstMatcher);
  NoDuplicateRefCntMember.registerMatcher(AstMatcher);
  NeedsNoVTableType.registerMatcher(AstMatcher);
  NonMemMovableTemplateArg.registerMatcher(AstMatcher);
  NonMemMovableMember.registerMatcher(AstMatcher);
  ExplicitImplicit.registerMatcher(AstMatcher);
  NoAutoType.registerMatcher(AstMatcher);
  NoExplicitMoveConstructor.registerMatcher(AstMatcher);
  RefCountedCopyConstructor.registerMatcher(AstMatcher);
  AssertAssignment.registerMatcher(AstMatcher);
  KungFuDeathGrip.registerMatcher(AstMatcher);
  SprintfLiteral.registerMatcher(AstMatcher);
  OverrideBaseCall.registerMatcher(AstMatcher);
  OverrideBaseCallUsage.registerMatcher(AstMatcher);
  NonParamInsideFunctionDecl.registerMatcher(AstMatcher);
}
