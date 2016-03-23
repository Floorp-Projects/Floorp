/** @file
    @brief Header wrapping the C99 standard `stdint` header.

    Must be c-safe!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_StdInt_h_GUID_C1AAF35C_C704_4DB7_14AC_615730C4619B
#define INCLUDED_StdInt_h_GUID_C1AAF35C_C704_4DB7_14AC_615730C4619B

/* IWYU pragma: begin_exports */

#if !defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <stdint.h>
#else
#include "MSStdIntC.h"
#endif

/* IWYU pragma: end_exports */

#endif
