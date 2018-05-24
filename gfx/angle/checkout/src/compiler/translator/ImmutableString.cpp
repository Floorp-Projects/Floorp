//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImmutableString.cpp: Wrapper for static or pool allocated char arrays, that are guaranteed to be
// valid and unchanged for the duration of the compilation.
//

#include "compiler/translator/ImmutableString.h"

std::ostream &operator<<(std::ostream &os, const sh::ImmutableString &str)
{
    return os.write(str.data(), str.length());
}

#if defined(_MSC_VER)
#pragma warning(disable : 4309)  // truncation of constant value
#endif

namespace sh
{

template <>
const size_t ImmutableString::FowlerNollVoHash<4>::kFnvPrime = 16777619u;

template <>
const size_t ImmutableString::FowlerNollVoHash<4>::kFnvOffsetBasis = 0x811c9dc5u;

template <>
const size_t ImmutableString::FowlerNollVoHash<8>::kFnvPrime =
    static_cast<size_t>(1099511628211ull);

template <>
const size_t ImmutableString::FowlerNollVoHash<8>::kFnvOffsetBasis =
    static_cast<size_t>(0xcbf29ce484222325ull);

uint32_t ImmutableString::hash32() const
{
    const char *data_ptr = data();
    uint32_t hash        = static_cast<uint32_t>(FowlerNollVoHash<4>::kFnvOffsetBasis);
    while ((*data_ptr) != '\0')
    {
        hash = hash ^ (*data_ptr);
        hash = hash * static_cast<uint32_t>(FowlerNollVoHash<4>::kFnvPrime);
        ++data_ptr;
    }
    return hash;
}

}  // namespace sh
