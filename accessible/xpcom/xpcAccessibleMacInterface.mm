/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleMacInterface.h"

#import "mozAccessible.h"

using namespace mozilla::a11y;

NS_IMPL_ISUPPORTS(xpcAccessibleMacInterface, nsIAccessibleMacInterface)

xpcAccessibleMacInterface::xpcAccessibleMacInterface(AccessibleOrProxy aObj) {
  mNativeObject = aObj.IsProxy()
                      ? GetNativeFromProxy(aObj.AsProxy())
                      : static_cast<AccessibleWrap*>(aObj.AsAccessible())->GetNativeObject();
  [mNativeObject retain];
}

xpcAccessibleMacInterface::xpcAccessibleMacInterface(id aNativeObj) : mNativeObject(aNativeObj) {
  [mNativeObject retain];
}

xpcAccessibleMacInterface::~xpcAccessibleMacInterface() { [mNativeObject release]; }

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeNames(nsTArray<nsString>& aAttributeNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSMutableSet* attributeNames =
      [NSMutableSet setWithArray:[mNativeObject accessibilityAttributeNames]];
  for (NSString* name in attributeNames) {
    nsAutoString attribName;
    nsCocoaUtils::GetStringForNSString(name, attribName);
    aAttributeNames.AppendElement(attribName);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetActionNames(nsTArray<nsString>& aActionNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSMutableSet* actionNames = [NSMutableSet setWithArray:[mNativeObject accessibilityActionNames]];
  for (NSString* name in actionNames) {
    nsAutoString actionName;
    nsCocoaUtils::GetStringForNSString(name, actionName);
    aActionNames.AppendElement(actionName);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::PerformAction(const nsAString& aActionName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  [mNativeObject accessibilityPerformAction:actionName];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeValue(const nsAString& aAttributeName, JSContext* aCx,
                                             JS::MutableHandleValue aResult) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  return NSObjectToJsValue([mNativeObject accessibilityAttributeValue:attribName], aCx, aResult);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

nsresult xpcAccessibleMacInterface::NSObjectToJsValue(id aObj, JSContext* aCx,
                                                      JS::MutableHandleValue aResult) {
  if (!aObj) {
    aResult.set(JS::NullValue());
  } else if ([aObj isKindOfClass:[NSString class]]) {
    nsAutoString strVal;
    nsCocoaUtils::GetStringForNSString((NSString*)aObj, strVal);
    if (!mozilla::dom::ToJSValue(aCx, strVal, aResult)) {
      return NS_ERROR_FAILURE;
    }
  } else if ([aObj isKindOfClass:[NSNumber class]]) {
    // If the type being held by the NSNumber is a BOOL set js value
    // to boolean. Otherwise use a double value.
    if (strcmp([(NSNumber*)aObj objCType], @encode(BOOL)) == 0) {
      if (!mozilla::dom::ToJSValue(aCx, [(NSNumber*)aObj boolValue], aResult)) {
        return NS_ERROR_FAILURE;
      }
    } else {
      if (!mozilla::dom::ToJSValue(aCx, [(NSNumber*)aObj doubleValue], aResult)) {
        return NS_ERROR_FAILURE;
      }
    }
  } else if ([aObj isKindOfClass:[NSValue class]] &&
             strcmp([(NSValue*)aObj objCType], @encode(NSPoint)) == 0) {
    NSPoint point = [(NSValue*)aObj pointValue];
    return NSObjectToJsValue(
        @[ [NSNumber numberWithDouble:point.x], [NSNumber numberWithDouble:point.y] ], aCx,
        aResult);
  } else if ([aObj isKindOfClass:[NSValue class]] &&
             strcmp([(NSValue*)aObj objCType], @encode(NSSize)) == 0) {
    NSSize size = [(NSValue*)aObj sizeValue];
    return NSObjectToJsValue(
        @[ [NSNumber numberWithDouble:size.width], [NSNumber numberWithDouble:size.height] ], aCx,
        aResult);
  } else if ([aObj respondsToSelector:@selector(accessibilityAttributeNames)]) {
    nsCOMPtr<nsIAccessibleMacInterface> obj = new xpcAccessibleMacInterface(aObj);
    return nsContentUtils::WrapNative(aCx, obj, &NS_GET_IID(nsIAccessibleMacInterface), aResult);
  } else if ([aObj isKindOfClass:[NSArray class]]) {
    NSArray* objArr = (NSArray*)aObj;

    JS::RootedVector<JS::Value> v(aCx);
    if (!v.resize([objArr count])) {
      return NS_ERROR_FAILURE;
    }
    for (size_t i = 0; i < [objArr count]; ++i) {
      nsresult rv = NSObjectToJsValue(objArr[i], aCx, v[i]);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    JSObject* arrayObj = JS::NewArrayObject(aCx, v);
    if (!arrayObj) {
      return NS_ERROR_FAILURE;
    }
    aResult.setObject(*arrayObj);
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}
