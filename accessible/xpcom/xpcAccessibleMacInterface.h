/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleMacInterface_h_
#define mozilla_a11y_xpcAccessibleMacInterface_h_

#include "mozilla/a11y/Accessible.h"
#include "nsIAccessibleMacInterface.h"

class nsIAccessibleMacInterface;

namespace mozilla {
namespace a11y {

class xpcAccessibleMacNSObjectWrapper : public nsIAccessibleMacNSObjectWrapper {
 public:
  explicit xpcAccessibleMacNSObjectWrapper(id aTextMarker);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEMACNSOBJECTWRAPPER

 protected:
  virtual ~xpcAccessibleMacNSObjectWrapper();

  id mNativeObject;
};

class xpcAccessibleMacInterface : public xpcAccessibleMacNSObjectWrapper,
                                  public nsIAccessibleMacInterface {
 public:
  // Construct an xpcAccessibleMacInterface using this
  // native object that conforms to the NSAccessibility protocol.
  explicit xpcAccessibleMacInterface(id aNativeObj)
      : xpcAccessibleMacNSObjectWrapper(aNativeObj) {}

  // Construct an xpcAccessibleMacInterface using the native object
  // associated with this accessible.
  explicit xpcAccessibleMacInterface(Accessible* aObj);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEMACINTERFACE

  // Convert an NSObject (which can be anything, string, number, array, etc.)
  // into a properly typed js value populated in the aResult handle.
  static nsresult NSObjectToJsValue(id aObj, JSContext* aCx,
                                    JS::MutableHandleValue aResult);

 protected:
  virtual ~xpcAccessibleMacInterface() {}

  // Return true if our native object responds to this selector and
  // if it implements isAccessibilitySelectorAllowed check that it returns true
  // too.
  bool SupportsSelector(SEL aSelector);

  // Convert a js value to an NSObject. This is called recursively for arrays.
  // If the conversion fails, aResult is set to an error and nil is returned.
  id JsValueToNSObject(JS::HandleValue aValue, JSContext* aCx,
                       nsresult* aResult);

  // Convert a js value to an NSValue NSObject. This is called
  // by JsValueToNSObject when encountering a JS object with
  // a "value" and "valueType" property.
  id JsValueToNSValue(JS::HandleObject aObject, JSContext* aCx,
                      nsresult* aResult);

  // Convert a js value to a specified NSObject. This is called
  // by JsValueToNSObject when encountering a JS object with
  // a "object" and "objcetType" property.
  id JsValueToSpecifiedNSObject(JS::HandleObject aObject, JSContext* aCx,
                                nsresult* aResult);

 private:
  xpcAccessibleMacInterface(const xpcAccessibleMacInterface&) = delete;
  xpcAccessibleMacInterface& operator=(const xpcAccessibleMacInterface&) =
      delete;
};

class xpcAccessibleMacEvent : public nsIAccessibleMacEvent {
 public:
  explicit xpcAccessibleMacEvent(id aNativeObj, id aData);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEMACEVENT;

  // This sends notifications via nsIObserverService to be consumed by our
  // mochitests. aNativeObj is a NSAccessibility protocol object,
  // and aNotification is a NSString.
  static void FireEvent(id aNativeObj, id aNotification, id aUserInfo);

 protected:
  virtual ~xpcAccessibleMacEvent();

  id mNativeObject;
  id mData;
};

}  // namespace a11y
}  // namespace mozilla

#endif
