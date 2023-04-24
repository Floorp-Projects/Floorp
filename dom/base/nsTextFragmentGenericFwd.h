/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFragmentGenericFwd_h__
#define nsTextFragmentGenericFwd_h__

#include <cstdint>
#include <xsimd/xsimd.hpp>

namespace mozilla {

template <class Arch>
int32_t FirstNon8Bit(const char16_t* str, const char16_t* end);

}  // namespace mozilla

#endif
