/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSPreferences.h"
#include <Carbon/Carbon.h>

using namespace mozilla::intl;

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
  MOZ_ASSERT(aLocaleList.IsEmpty());

  // Get string representation of user's current locale
  CFLocaleRef userLocaleRef = ::CFLocaleCopyCurrent();
  CFStringRef userLocaleStr = ::CFLocaleGetIdentifier(userLocaleRef);

  AutoTArray<UniChar, 32> buffer;
  int size = ::CFStringGetLength(userLocaleStr);
  buffer.SetLength(size);

  CFRange range = ::CFRangeMake(0, size);
  ::CFStringGetCharacters(userLocaleStr, range, buffer.Elements());

  // Convert the locale string to the format that Mozilla expects
  NS_LossyConvertUTF16toASCII locale(
      reinterpret_cast<const char16_t*>(buffer.Elements()), buffer.Length());

  CFRelease(userLocaleRef);

  if (CanonicalizeLanguageTag(locale)) {
    aLocaleList.AppendElement(locale);
    return true;
  }

  return false;
}
