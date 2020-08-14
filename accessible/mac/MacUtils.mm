/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MacUtils.h"

#include "Accessible.h"
#include "nsCocoaUtils.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "nsIPersistentProperties2.h"

namespace mozilla {
namespace a11y {
namespace utils {

// convert an array of Gecko accessibles to an NSArray of native accessibles
NSMutableArray* ConvertToNSArray(nsTArray<Accessible*>& aArray) {
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    Accessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

// convert an array of Gecko proxy accessibles to an NSArray of native accessibles
NSMutableArray* ConvertToNSArray(nsTArray<ProxyAccessible*>& aArray) {
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    ProxyAccessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

/**
 * Get a localized string from the a11y string bundle.
 * Return nil if not found.
 */
NSString* LocalizedString(const nsString& aString) {
  nsString text;

  Accessible::TranslateString(aString, text);

  return text.IsEmpty() ? nil : nsCocoaUtils::ToNSString(text);
}

NSString* GetAccAttr(mozAccessible* aNativeAccessible, const char* aAttrName) {
  nsAutoString result;
  if (Accessible* acc = [aNativeAccessible geckoAccessible].AsAccessible()) {
    nsCOMPtr<nsIPersistentProperties> attributes = acc->Attributes();
    attributes->GetStringProperty(nsCString(aAttrName), result);
  } else if (ProxyAccessible* proxy = [aNativeAccessible geckoAccessible].AsProxy()) {
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
