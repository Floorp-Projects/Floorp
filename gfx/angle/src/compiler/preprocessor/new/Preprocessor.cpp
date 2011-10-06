//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Preprocessor.h"

#include "compiler/debug.h"
#include "Context.h"

namespace pp
{

bool Preprocessor::process(int count,
                           const char* const string[],
                           const int length[])
{
    ASSERT((count >=0) && (string != NULL));
    if ((count < 0) || (string == NULL))
        return false;

    clearResults();
    Context context(count, string, length, &mTokens);
    if (!initLexer(&context))
        return false;

    bool success = parse(&context);

    destroyLexer(&context);
    return success;
}

void Preprocessor::clearResults()
{
    for (TokenVector::iterator i = mTokens.begin(); i != mTokens.end(); ++i)
        delete (*i);

    mTokens.clear();
}

}  // namespace pp

