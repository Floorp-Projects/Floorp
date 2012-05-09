//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_LEXER_H_
#define COMPILER_PREPROCESSOR_LEXER_H_

#include <memory>

#include "pp_utils.h"

namespace pp
{

class Input;
struct Token;

class Lexer
{
  public:
    Lexer();
    ~Lexer();

    bool init(int count, const char* const string[], const int length[]);

    int lex(Token* token);

  private:
    PP_DISALLOW_COPY_AND_ASSIGN(Lexer);
    bool initLexer();
    void destroyLexer();

    void* mHandle;  // Lexer handle.
    bool mLeadingSpace;
    std::auto_ptr<Input> mInput;  // Input buffer.
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_LEXER_H_

