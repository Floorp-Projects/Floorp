//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_PREPROCESSOR_H_
#define COMPILER_PREPROCESSOR_PREPROCESSOR_H_

#include "Lexer.h"
#include "pp_utils.h"

namespace pp
{

class Preprocessor
{
  public:
    Preprocessor() { }

    bool init(int count, const char* const string[], const int length[]);
    int lex(Token* token);

  private:
    PP_DISALLOW_COPY_AND_ASSIGN(Preprocessor);
    Lexer mLexer;
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_PREPROCESSOR_H_

