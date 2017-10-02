//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
#define COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_

#include <GLSLANG/ShaderLang.h>

#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/IntermNode.h"

namespace sh
{
class TSymbolTable;

typedef std::vector<sh::ShaderVariable> InitVariableList;

// Return a sequence of assignment operations to initialize "initializedSymbol". initializedSymbol
// may be an array, struct or any combination of these, as long as it contains only basic types.
TIntermSequence *CreateInitCode(const TIntermTyped *initializedSymbol);

// Initialize all uninitialized local variables, so that undefined behavior is avoided.
void InitializeUninitializedLocals(TIntermBlock *root, int shaderVersion);

// This function can initialize all the types that CreateInitCode is able to initialize. All
// variables must be globals which can be found in the symbol table. For now it is used for the
// following two scenarios:
//   1. Initializing gl_Position;
//   2. Initializing output variables referred to in the shader source.
// Note: The type of each lvalue in an initializer is retrieved from the symbol table. gl_FragData
// requires special handling because the number of indices which can be initialized is determined by
// enabled extensions.
void InitializeVariables(TIntermBlock *root,
                         const InitVariableList &vars,
                         const TSymbolTable &symbolTable,
                         int shaderVersion,
                         const TExtensionBehavior &extensionBehavior);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
