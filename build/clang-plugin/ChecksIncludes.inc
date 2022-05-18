/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The list of #include directives necessary for the checker classes that
// are compatible with clang-tidy.

#include "ArithmeticArgChecker.h"
#include "AssertAssignmentChecker.h"
#include "CanRunScriptChecker.h"
#include "DanglingOnTemporaryChecker.h"
#include "ExplicitImplicitChecker.h"
#include "ExplicitOperatorBoolChecker.h"
#ifdef TARGET_IS_WINDOWS
#include "LoadLibraryUsageChecker.h"
#include "FopenUsageChecker.h"
#endif
#include "JSHandleRootedTypedefChecker.h"
#include "KungFuDeathGripChecker.h"
#include "KnownLiveChecker.h"
#include "MustOverrideChecker.h"
#include "MustReturnFromCallerChecker.h"
#include "NaNExprChecker.h"
#include "NoPrincipalGetURI.h"
#include "NeedsNoVTableTypeChecker.h"
#include "NoAddRefReleaseOnReturnChecker.h"
#include "NoAutoTypeChecker.h"
#include "NoDuplicateRefCntMemberChecker.h"
#include "NoExplicitMoveConstructorChecker.h"
#include "NoNewThreadsChecker.h"
#include "NonMemMovableMemberChecker.h"
#include "NonMemMovableTemplateArgChecker.h"
#include "NonParamInsideFunctionDeclChecker.h"
#include "NonTrivialTypeInFfiChecker.h"
#include "NoUsingNamespaceMozillaJavaChecker.h"
#include "OverrideBaseCallChecker.h"
#include "OverrideBaseCallUsageChecker.h"
#include "ParamTraitsEnumChecker.h"
#include "RefCountedCopyConstructorChecker.h"
#include "RefCountedInsideLambdaChecker.h"
#include "ScopeChecker.h"
#include "SprintfLiteralChecker.h"
#include "TemporaryLifetimeBoundChecker.h"
#include "TrivialCtorDtorChecker.h"
#include "TrivialDtorChecker.h"
