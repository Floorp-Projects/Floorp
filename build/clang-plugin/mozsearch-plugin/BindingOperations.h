/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/JSON.h>

void findBindingToJavaClass(clang::ASTContext &C, clang::CXXRecordDecl &klass);
void findBoundAsJavaClasses(clang::ASTContext &C, clang::CXXRecordDecl &klass);
void findBindingToJavaFunction(clang::ASTContext &C, clang::FunctionDecl &function);
void findBindingToJavaMember(clang::ASTContext &C, clang::CXXMethodDecl &method);
void findBindingToJavaConstant(clang::ASTContext &C, clang::VarDecl &field);

void emitBindingAttributes(llvm::json::OStream &json, const clang::Decl &decl);
