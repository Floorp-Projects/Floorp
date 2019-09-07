//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteAtomicCounters: Change atomic counter buffers to storage buffers, with atomic counter
// variables being offsets into the uint array of that storage buffer.

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITEATOMICCOUNTERS_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITEATOMICCOUNTERS_H_

namespace sh
{
class TIntermBlock;
class TSymbolTable;
class TVariable;

void RewriteAtomicCounters(TIntermBlock *root, TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITEATOMICCOUNTERS_H_
