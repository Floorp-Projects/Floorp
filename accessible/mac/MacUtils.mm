/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MacUtils.h"

#include "LocalAccessible.h"
#include "nsCocoaUtils.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "nsIPersistentProperties2.h"

namespace mozilla {
namespace a11y {
namespace utils {

// convert an array of Gecko accessibles to an NSArray of native accessibles
NSArray<mozAccessible*>* ConvertToNSArray(nsTArray<LocalAccessible*>& aArray) {
  NSMutableArray* nativeArray = [[[NSMutableArray alloc] init] autorelease];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    LocalAccessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative)
      [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

// convert an array of Gecko proxy accessibles to an NSArray of native
// accessibles
NSArray<mozAccessible*>* ConvertToNSArray(nsTArray<RemoteAccessible*>& aArray) {
  NSMutableArray* nativeArray = [[[NSMutableArray alloc] init] autorelease];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    RemoteAccessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative)
      [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

/**
 * Get a localized string from the a11y string bundle.
 * Return nil if not found.
 */
NSString* LocalizedString(const nsString& aString) {
  nsString text;

  LocalAccessible::TranslateString(aString, text);

  return text.IsEmpty() ? nil : nsCocoaUtils::ToNSString(text);
}

NSString* GetAccAttr(mozAccessible* aNativeAccessible, const char* aAttrName) {
  nsAutoString result;
  if (LocalAccessible* acc =
          [aNativeAccessible geckoAccessible].AsAccessible()) {
    nsCOMPtr<nsIPersistentProperties> attributes = acc->Attributes();
    attributes->GetStringProperty(nsCString(aAttrName), result);
  } else if (RemoteAccessible* proxy =
                 [aNativeAccessible geckoAccessible].AsProxy()) {
    AutoTArray<Attribute, 10> attrs;
    proxy->Attributes(&attrs);
    for (size_t i = 0; i < attrs.Length(); i++) {
      if (attrs.ElementAt(i).Name() == aAttrName) {
        result = attrs.ElementAt(i).Value();
        break;
      }
    }
  }

  if (!result.IsEmpty()) {
    return nsCocoaUtils::ToNSString(result);
  }

  return nil;
}
}
}
}
