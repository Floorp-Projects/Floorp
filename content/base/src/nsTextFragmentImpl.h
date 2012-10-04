/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFragmentImpl_h__
#define nsTextFragmentImpl_h__

#include "mozilla/StandardInteger.h"

template<size_t size> struct Non8BitParameters;
template<> struct Non8BitParameters<4> {
  static inline size_t mask() { return 0xff00ff00; }
  static inline uint32_t alignMask() { return 0x3; }
  static inline uint32_t numUnicharsPerWord() { return 2; }
};

template<> struct Non8BitParameters<8> {
  static inline size_t mask() { return 0xff00ff00ff00ff00; }
  static inline uint32_t alignMask() { return 0x7; }
  static inline uint32_t numUnicharsPerWord() { return 4; }
};

#endif
