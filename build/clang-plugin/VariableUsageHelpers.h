/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <vector>

#ifndef VariableUsageHelpers_h__
#define VariableUsageHelpers_h__

#include "plugin.h"

/// Returns a list of the statements where the given declaration is used as an
/// rvalue (within the provided function).
///
/// WARNING: incomplete behaviour/implementation for general-purpose use outside
/// of escapesFunction(). This only detects very basic usages (see
/// implementation for more details).
std::vector<const Stmt*>
getUsageAsRvalue(const ValueDecl* ValueDeclaration,
                 const FunctionDecl* FuncDecl);

/// Returns a (statement, decl) tuple if an argument from a call expression
/// escapes the function scope through globals/statics/other things. The
/// statement is where the value escapes the function, while the declaration
/// points to what it escapes through. If the argument doesn't escape the
/// function, the tuple will only contain nullptrs.
/// If the analysis runs into an unexpected error or into an unimplemented
/// configuration, this returns NoneType.
///
/// WARNING: incomplete behaviour/implementation for general-purpose use outside
/// of DanglingOnTemporaryChecker. This only covers a limited set of cases,
/// mainly in terms of arguments and parameter types.
Optional<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const CallExpr* Call);

#endif
