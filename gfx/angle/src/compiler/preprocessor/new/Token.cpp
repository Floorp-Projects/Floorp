//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Token.h"

namespace pp
{

std::ostream& operator<<(std::ostream& out, const Token& token)
{
    if (token.hasLeadingSpace())
        out << " ";

    out << token.value;
    return out;
}

}  // namespace pp
