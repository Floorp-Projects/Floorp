//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_CONTEXT_H_
#define COMPILER_PREPROCESSOR_CONTEXT_H_

#include "Input.h"
#include "Macro.h"
#include "Token.h"

namespace pp
{

struct Context
{
    Context(int count, const char* const string[], const int length[],
            TokenVector* output);

    Input input;
    TokenVector* output;

    void* lexer;  // Lexer handle.
    MacroSet macros;  // Defined macros.
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_CONTEXT_H_

