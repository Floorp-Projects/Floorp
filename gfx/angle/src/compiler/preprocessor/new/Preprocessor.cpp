//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Preprocessor.h"

namespace pp
{

bool Preprocessor::init(int count,
                        const char* const string[],
                        const int length[])
{
    return mLexer.init(count, string, length);
}

int Preprocessor::lex(Token* token)
{
    return mLexer.lex(token);
}

}  // namespace pp

