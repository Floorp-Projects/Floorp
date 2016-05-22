/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BigEndianInts_h
#define mozilla_BigEndianInts_h

#include "mozilla/EndianUtils.h"

namespace mozilla {

#pragma pack(push, 1)

struct BigEndianUint16
{
#ifdef __SUNPRO_CC
  BigEndianUint16& operator=(const uint16_t aValue)
  {
    value = NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT BigEndianUint16(const uint16_t aValue)
  {
    value = NativeEndian::swapToBigEndian(aValue);
  }
#endif

  operator uint16_t() const
  {
    return NativeEndian::swapFromBigEndian(value);
  }

  friend inline bool
  operator==(const BigEndianUint16& lhs, const BigEndianUint16& rhs)
  {
    return lhs.value == rhs.value;
  }

  friend inline bool
  operator!=(const BigEndianUint16& lhs, const BigEndianUint16& rhs)
  {
    return !(lhs == rhs);
  }

private:
  uint16_t value;
};

struct BigEndianUint32
{
#ifdef __SUNPRO_CC
  BigEndianUint32& operator=(const uint32_t aValue)
  {
    value = NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT BigEndianUint32(const uint32_t aValue)
  {
    value = NativeEndian::swapToBigEndian(aValue);
  }
#endif

  operator uint32_t() const
  {
    return NativeEndian::swapFromBigEndian(value);
  }

private:
  uint32_t  value;
};

#pragma pack(pop)

} // mozilla

#endif // mozilla_BigEndianInts_h
