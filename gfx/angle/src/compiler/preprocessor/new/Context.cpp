//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Context.h"

namespace pp
{

Context::Context(int count, const char* const string[], const int length[],
                 TokenVector* output)
    : input(count, string, length),
      output(output),
      lexer(NULL)
{
}

}  // namespace pp

