//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_MACRO_H_
#define COMPILER_PREPROCESSOR_MACRO_H_

#include <map>
#include <string>

#include "Token.h"

namespace pp
{

struct Macro
{
    enum Type
    {
        kTypeObj,
        kTypeFunc
    };
    Type type;
    std::string identifier;
    TokenVector parameters;
    TokenVector replacements;
};
typedef std::map<std::string, Macro> MacroSet;

}  // namespace pp
#endif COMPILER_PREPROCESSOR_MACRO_H_

