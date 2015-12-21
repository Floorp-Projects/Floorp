/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_u16string_h
#define mozilla_gfx_u16string_h

#include <string>

#include "mozilla/Char16.h"

namespace mozilla {

#if defined(_MSC_VER)
typedef std::u16string u16string;
#else
typedef std::basic_string<char16_t> u16string;
#endif

} // mozilla

#endif // mozilla_gfx_u16string_h
