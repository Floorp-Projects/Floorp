/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInlines_h
#define mozilla_StyleSheetInlines_h

#include "mozilla/ServoStyleSheet.h"
#include "mozilla/CSSStyleSheet.h"

namespace mozilla {

CSSStyleSheet&
StyleSheet::AsGecko()
{
  MOZ_ASSERT(IsGecko());
  return *static_cast<CSSStyleSheet*>(this);
}

ServoStyleSheet&
StyleSheet::AsServo()
{
  MOZ_ASSERT(IsServo());
  return *static_cast<ServoStyleSheet*>(this);
}

StyleSheetHandle
StyleSheet::AsHandle()
{
  if (IsServo()) {
    return &AsServo();
  }
  return &AsGecko();
}

}

#endif // mozilla_StyleSheetInlines_h
