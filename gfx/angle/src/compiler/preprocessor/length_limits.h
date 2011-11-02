//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// length_limits.h
//

#if !defined(__LENGTH_LIMITS_H)
#define __LENGTH_LIMITS_H 1

// These constants are factored out from the rest of the headers to
// make it easier to reference them from the compiler sources.

// These lengths do not include the NULL terminator.
// see bug 675625: NVIDIA driver crash with lengths >= 253
// this is only an interim fix, the real fix is name mapping, see ANGLE bug 144 / r619
#define MAX_SYMBOL_NAME_LEN 250
#define MAX_STRING_LEN 511

#endif // !(defined(__LENGTH_LIMITS_H)
