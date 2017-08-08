/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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

/// This is the error enumeration for escapesFunction(), describing all the
/// possible error cases.
enum class EscapesFunctionError {
  ConstructorDeclNotFound = 1,
  FunctionDeclNotFound,
  FunctionIsBuiltin,
  FunctionIsVariadic,
  ExprNotInCall,
  NoParamForArg,
  ArgAndParamNotPointers
};

/// Required by the std::error_code system to convert our enum into a general
/// error code.
std::error_code make_error_code(EscapesFunctionError);

/// Returns a (statement, decl) tuple if an argument from an argument list
/// escapes the function scope through globals/statics/other things. The
/// statement is where the value escapes the function, while the declaration
/// points to what it escapes through. If the argument doesn't escape the
/// function, the tuple will only contain nullptrs.
/// If the analysis runs into an unexpected error or into an unimplemented
/// configuration, it will return an error_code of type EscapesFunctionError
/// representing the precise issue.
///
/// WARNING: incomplete behaviour/implementation for general-purpose use outside
/// of DanglingOnTemporaryChecker. This only covers a limited set of cases,
/// mainly in terms of arguments and parameter types.
ErrorOr<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const FunctionDecl *FuncDecl,
                const Expr* const* Arguments, unsigned NumArgs);

/// Helper function taking a call expression.
ErrorOr<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const CallExpr* Call);

/// Helper function taking a construct expression.
ErrorOr<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const CXXConstructExpr* Construct);

/// Helper function taking an operator call expression.
ErrorOr<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const CXXOperatorCallExpr* OpCall);

#endif
