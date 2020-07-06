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
#include "clang/Analysis/CFG.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <iterator>
#include <memory>

#define CLANG_VERSION_FULL (CLANG_VERSION_MAJOR * 100 + CLANG_VERSION_MINOR)

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

#if CLANG_VERSION_FULL >= 306
typedef std::unique_ptr<ASTConsumer> ASTConsumerPtr;
#else
typedef ASTConsumer *ASTConsumerPtr;
#endif

#if CLANG_VERSION_FULL < 800
// Starting with Clang 8.0 some basic functions have been renamed
#define getBeginLoc getLocStart
#define getEndLoc getLocEnd
#endif

// In order to support running our checks using clang-tidy, we implement a
// source compatible base check class called BaseCheck, and we use the
// preprocessor to decide which base class to pick.
#ifdef CLANG_TIDY
#if CLANG_VERSION_FULL >= 900
#include "../ClangTidyCheck.h"
#else
#include "../ClangTidy.h"
#endif
typedef clang::tidy::ClangTidyCheck BaseCheck;
typedef clang::tidy::ClangTidyContext ContextType;
#else
#include "BaseCheck.h"
#endif

#endif // plugin_h__
