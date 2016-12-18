/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef plugin_h__
#define plugin_h__

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <memory>
#include <iterator>

#define CLANG_VERSION_FULL (CLANG_VERSION_MAJOR * 100 + CLANG_VERSION_MINOR)

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

#if CLANG_VERSION_FULL >= 306
typedef std::unique_ptr<ASTConsumer> ASTConsumerPtr;
#else
typedef ASTConsumer *ASTConsumerPtr;
#endif

#ifndef HAVE_NEW_ASTMATCHER_NAMES
// In clang 3.8, a number of AST matchers were renamed to better match the
// respective AST node.  We use the new names, and #define them to the old
// ones for compatibility with older versions.
#define cxxConstructExpr constructExpr
#define cxxConstructorDecl constructorDecl
#define cxxMethodDecl methodDecl
#define cxxNewExpr newExpr
#define cxxRecordDecl recordDecl
#endif

#ifndef HAS_ACCEPTS_IGNORINGPARENIMPCASTS
#define hasIgnoringParenImpCasts(x) has(x)
#else
// Before clang 3.9 "has" would behave like has(ignoringParenImpCasts(x)),
// however doing that explicitly would not compile.
#define hasIgnoringParenImpCasts(x) has(ignoringParenImpCasts(x))
#endif

#endif

// In order to support running our checks using clang-tidy, we implement a source
// compatible base check class called BaseCheck, and we use the preprocessor to
// decide which base class to pick.
#ifdef CLANG_TIDY
#include "../ClangTidy.h"
typedef clang::tidy::ClangTidyCheck BaseCheck;
typedef clang::tidy::ClangTidyContext ContextType;
#else
#include "BaseCheck.h"
#endif
