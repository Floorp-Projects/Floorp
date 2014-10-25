/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFragmentImpl_h__
#define nsTextFragmentImpl_h__

#include <stdint.h>

template<size_t size> struct Non8BitParameters;
template<> struct Non8BitParameters<4> {
  static inline size_t mask() { return 0xff00ff00; }
  static inline uint32_t alignMask() { return 0x3; }
  static inline uint32_t numUnicharsPerWord() { return 2; }
};

template<> struct Non8BitParameters<8> {
  static inline size_t mask() {
    static const uint64_t maskAsUint64 = 0xff00ff00ff00ff00ULL;
    // We have to explicitly cast this 64-bit value to a size_t, or else
    // compilers for 32-bit platforms will warn about it being too large to fit
    // in the size_t return type. (Fortunately, this code isn't actually
    // invoked on 32-bit platforms -- they'll use the <4> specialization above.
    // So it is, in fact, OK that this value is too large for a 32-bit size_t.)
    return (size_t)maskAsUint64;
  }
  static inline uint32_t alignMask() { return 0x7; }
  static inline uint32_t numUnicharsPerWord() { return 4; }
};

#endif
