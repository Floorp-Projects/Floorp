/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleMacInterface.h"

#include "nsCocoaUtils.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/dom/ToJSValue.h"

#import "mozAccessible.h"

using namespace mozilla::a11y;

// xpcAccessibleMacNSObjectWrapper

NS_IMPL_ISUPPORTS(xpcAccessibleMacNSObjectWrapper, nsIAccessibleMacNSObjectWrapper)

xpcAccessibleMacNSObjectWrapper::xpcAccessibleMacNSObjectWrapper(id aNativeObj)
    : mNativeObject(aNativeObj) {
  [mNativeObject retain];
}

xpcAccessibleMacNSObjectWrapper::~xpcAccessibleMacNSObjectWrapper() { [mNativeObject release]; }

id xpcAccessibleMacNSObjectWrapper::GetNativeObject() const { return mNativeObject; }

// xpcAccessibleMacInterface

NS_IMPL_ISUPPORTS_INHERITED(xpcAccessibleMacInterface, xpcAccessibleMacNSObjectWrapper,
                            nsIAccessibleMacInterface)

xpcAccessibleMacInterface::xpcAccessibleMacInterface(AccessibleOrProxy aObj)
    : xpcAccessibleMacNSObjectWrapper(GetNativeFromGeckoAccessible(aObj)) {}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeNames(nsTArray<nsString>& aAttributeNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (NSString* name in [mNativeObject accessibilityAttributeNames]) {
    nsAutoString attribName;
    nsCocoaUtils::GetStringForNSString(name, attribName);
    aAttributeNames.AppendElement(attribName);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetParameterizedAttributeNames(nsTArray<nsString>& aAttributeNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (NSString* name in [mNativeObject accessibilityParameterizedAttributeNames]) {
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

  for (NSString* name in [mNativeObject accessibilityActionNames]) {
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

  NSString* actionName = nsCocoaUtils::ToNSString(aActionName);
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

NS_IMETHODIMP
xpcAccessibleMacInterface::IsAttributeSettable(const nsAString& aAttributeName, bool* aIsSettable) {
  NS_ENSURE_ARG_POINTER(aIsSettable);

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  if ([mNativeObject respondsToSelector:@selector(accessibilityIsAttributeSettable:)]) {
    *aIsSettable = [mNativeObject accessibilityIsAttributeSettable:attribName];
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
xpcAccessibleMacInterface::SetAttributeValue(const nsAString& aAttributeName,
                                             JS::HandleValue aAttributeValue, JSContext* aCx) {
  nsresult rv = NS_OK;
  id obj = JsValueToNSObject(aAttributeValue, aCx, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  if ([mNativeObject respondsToSelector:@selector(accessibilitySetValue:forAttribute:)]) {
    // The NSObject has an attribute setter, call that.
    [mNativeObject accessibilitySetValue:obj forAttribute:attribName];
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetParameterizedAttributeValue(const nsAString& aAttributeName,
                                                          JS::HandleValue aParameter,
                                                          JSContext* aCx,
                                                          JS::MutableHandleValue aResult) {
  nsresult rv = NS_OK;
  id paramObj = JsValueToNSObject(aParameter, aCx, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  return NSObjectToJsValue(
      [mNativeObject accessibilityAttributeValue:attribName forParameter:paramObj], aCx, aResult);

  return NS_OK;
}

bool xpcAccessibleMacInterface::SupportsSelector(SEL aSelector) {
  // return true if we have this selector, and if isAccessibilitySelectorAllowed
  // is implemented too whether it is "allowed".
  return [mNativeObject respondsToSelector:aSelector] &&
         (![mNativeObject respondsToSelector:@selector(isAccessibilitySelectorAllowed:selector:)] ||
          [mNativeObject isAccessibilitySelectorAllowed:aSelector]);
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
  } else if ([aObj isKindOfClass:[NSValue class]] &&
             strcmp([(NSValue*)aObj objCType], @encode(NSRange)) == 0) {
    NSRange range = [(NSValue*)aObj rangeValue];
    return NSObjectToJsValue(@[ @(range.location), @(range.length) ], aCx, aResult);
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
  } else if ([aObj respondsToSelector:@selector(isAccessibilityElement)]) {
    // We expect all of our accessibility objects to implement isAccessibilityElement
    // at the very least. If it is implemented we will assume its an accessibility object.
    nsCOMPtr<nsIAccessibleMacInterface> obj = new xpcAccessibleMacInterface(aObj);
    return nsContentUtils::WrapNative(aCx, obj, &NS_GET_IID(nsIAccessibleMacInterface), aResult);
  } else {
    // If this is any other kind of NSObject, just wrap it and return it.
    // It will be opaque and immutable on the JS side, but it can be
    // brought back to us in an argument.
    nsCOMPtr<nsIAccessibleMacNSObjectWrapper> obj = new xpcAccessibleMacNSObjectWrapper(aObj);
    return nsContentUtils::WrapNative(aCx, obj, &NS_GET_IID(nsIAccessibleMacNSObjectWrapper),
                                      aResult);
  }

  return NS_OK;
}

id xpcAccessibleMacInterface::JsValueToNSObject(JS::HandleValue aValue, JSContext* aCx,
                                                nsresult* aResult) {
  *aResult = NS_OK;
  if (aValue.isInt32()) {
    return [NSNumber numberWithInteger:aValue.toInt32()];
  } else if (aValue.isBoolean()) {
    return [NSNumber numberWithBool:aValue.toBoolean()];
  } else if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, aValue.toObjectOrNull());

    bool isArray;
    JS::IsArrayObject(aCx, obj, &isArray);
    if (isArray) {
      // If this is an array, we construct an NSArray and insert the js
      // array's elements by recursively calling this function.
      uint32_t len;
      JS::GetArrayLength(aCx, obj, &len);
      NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
      for (uint32_t i = 0; i < len; i++) {
        JS::RootedValue v(aCx);
        JS_GetElement(aCx, obj, i, &v);
        [array addObject:JsValueToNSObject(v, aCx, aResult)];
        NS_ENSURE_SUCCESS(*aResult, nil);
      }
      return array;
    }

    bool hasValueType;
    bool hasValue;
    JS_HasOwnProperty(aCx, obj, "valueType", &hasValueType);
    JS_HasOwnProperty(aCx, obj, "value", &hasValue);
    if (hasValueType && hasValue) {
      // A js object representin an NSValue looks like this:
      // { valueType: "NSRange", value: [1, 3] }
      return JsValueToNSValue(obj, aCx, aResult);
    }

    // This may be another nsIAccessibleMacInterface instance.
    // If so, return the wrapped NSObject.
    nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();

    nsCOMPtr<nsIXPConnectWrappedNative> wrappedObj;
    nsresult rv = xpc->GetWrappedNativeOfJSObject(aCx, obj, getter_AddRefs(wrappedObj));
    NS_ENSURE_SUCCESS(rv, nil);
    nsCOMPtr<nsIAccessibleMacNSObjectWrapper> macObjIface = do_QueryInterface(wrappedObj->Native());
    return macObjIface->GetNativeObject();
  }

  *aResult = NS_ERROR_FAILURE;
  return nil;
}

id xpcAccessibleMacInterface::JsValueToNSValue(JS::HandleObject aObject, JSContext* aCx,
                                               nsresult* aResult) {
  *aResult = NS_ERROR_FAILURE;
  JS::RootedValue valueTypeValue(aCx);
  if (!JS_GetProperty(aCx, aObject, "valueType", &valueTypeValue)) {
    NS_WARNING("Could not get valueType");
    return nil;
  }

  JS::RootedValue valueValue(aCx);
  if (!JS_GetProperty(aCx, aObject, "value", &valueValue)) {
    NS_WARNING("Could not get value");
    return nil;
  }

  nsAutoJSString valueType;
  if (!valueTypeValue.isString() || !valueType.init(aCx, valueTypeValue)) {
    NS_WARNING("valueType is not a string");
    return nil;
  }

  bool isArray;
  JS::IsArrayObject(aCx, valueValue, &isArray);
  if (!isArray) {
    NS_WARNING("value is not an array");
    return nil;
  }

  JS::Rooted<JSObject*> value(aCx, valueValue.toObjectOrNull());

  if (valueType.EqualsLiteral("NSRange")) {
    uint32_t len;
    JS::GetArrayLength(aCx, value, &len);
    if (len != 2) {
      NS_WARNING("Expected a 2 member array");
      return nil;
    }

    JS::RootedValue locationValue(aCx);
    JS_GetElement(aCx, value, 0, &locationValue);
    JS::RootedValue lengthValue(aCx);
    JS_GetElement(aCx, value, 1, &lengthValue);
    if (!locationValue.isInt32() || !lengthValue.isInt32()) {
      NS_WARNING("Expected an array of integers");
      return nil;
    }

    *aResult = NS_OK;
    return [NSValue valueWithRange:NSMakeRange(locationValue.toInt32(), lengthValue.toInt32())];
  }

  return nil;
}

void xpcAccessibleMacInterface::FireEvent(id aNativeObj, id aNotification) {
  if (nsCOMPtr<nsIObserverService> obsService = services::GetObserverService()) {
    nsCOMPtr<nsISimpleEnumerator> observers;
    // Get all observers for the mac event topic.
    obsService->EnumerateObservers(NS_ACCESSIBLE_MAC_EVENT_TOPIC, getter_AddRefs(observers));
    if (observers) {
      bool hasObservers = false;
      observers->HasMoreElements(&hasObservers);
      // If we have observers, notify them.
      if (hasObservers) {
        nsCOMPtr<nsIAccessibleMacInterface> xpcIface = new xpcAccessibleMacInterface(aNativeObj);
        nsAutoString notificationStr;
        nsCocoaUtils::GetStringForNSString(aNotification, notificationStr);
        obsService->NotifyObservers(xpcIface, NS_ACCESSIBLE_MAC_EVENT_TOPIC, notificationStr.get());
      }
    }
  }
}
