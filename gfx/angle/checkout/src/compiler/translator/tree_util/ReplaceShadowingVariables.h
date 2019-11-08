//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceShadowingVariables.h: Find any variables that are redefined within a nested
// scope and replace them with a newly named variable.

#ifndef COMPILER_TRANSLATOR_TREEUTIL_REPLACESHADOWINGVARIABLES_H_
#define COMPILER_TRANSLATOR_TREEUTIL_REPLACESHADOWINGVARIABLES_H_

namespace sh
{

class TIntermBlock;
class TSymbolTable;

void ReplaceShadowingVariables(TIntermBlock *root, TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEUTIL_REPLACESHADOWINGVARIABLES_H_
