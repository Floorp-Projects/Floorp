/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DiagnosticsMatcher_h__
#define DiagnosticsMatcher_h__

#include "ArithmeticArgChecker.h"
#include "AssertAssignmentChecker.h"
#include "ExplicitImplicitChecker.h"
#include "ExplicitOperatorBoolChecker.h"
#include "KungFuDeathGripChecker.h"
#include "NaNExprChecker.h"
#include "NeedsNoVTableTypeChecker.h"
#include "NoAddRefReleaseOnReturnChecker.h"
#include "NoAutoTypeChecker.h"
#include "NoDuplicateRefCntMemberChecker.h"
#include "NoExplicitMoveConstructorChecker.h"
#include "NonMemMovableMemberChecker.h"
#include "NonMemMovableTemplateArgChecker.h"
#include "NonParamInsideFunctionDeclChecker.h"
#include "OverrideBaseCallChecker.h"
#include "OverrideBaseCallUsageChecker.h"
#include "RefCountedCopyConstructorChecker.h"
#include "RefCountedInsideLambdaChecker.h"
#include "ScopeChecker.h"
#include "SprintfLiteralChecker.h"
#include "TrivialCtorDtorChecker.h"

class DiagnosticsMatcher {
public:
  DiagnosticsMatcher();

  ASTConsumerPtr makeASTConsumer() { return AstMatcher.newASTConsumer(); }

private:
  ScopeChecker Scope;
  ArithmeticArgChecker ArithmeticArg;
  TrivialCtorDtorChecker TrivialCtorDtor;
  NaNExprChecker NaNExpr;
  NoAddRefReleaseOnReturnChecker NoAddRefReleaseOnReturn;
  RefCountedInsideLambdaChecker RefCountedInsideLambda;
  ExplicitOperatorBoolChecker ExplicitOperatorBool;
  NoDuplicateRefCntMemberChecker NoDuplicateRefCntMember;
  NeedsNoVTableTypeChecker NeedsNoVTableType;
  NonMemMovableTemplateArgChecker NonMemMovableTemplateArg;
  NonMemMovableMemberChecker NonMemMovableMember;
  ExplicitImplicitChecker ExplicitImplicit;
  NoAutoTypeChecker NoAutoType;
  NoExplicitMoveConstructorChecker NoExplicitMoveConstructor;
  RefCountedCopyConstructorChecker RefCountedCopyConstructor;
  AssertAssignmentChecker AssertAssignment;
  KungFuDeathGripChecker KungFuDeathGrip;
  SprintfLiteralChecker SprintfLiteral;
  OverrideBaseCallChecker OverrideBaseCall;
  OverrideBaseCallUsageChecker OverrideBaseCallUsage;
  NonParamInsideFunctionDeclChecker NonParamInsideFunctionDecl;
  MatchFinder AstMatcher;
};

#endif
