/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleMacInterface_h_
#define mozilla_a11y_xpcAccessibleMacInterface_h_

#include "nsIAccessibleMacInterface.h"

#include "AccessibleOrProxy.h"

class nsIAccessibleMacInterface;

namespace mozilla {
namespace a11y {

class xpcAccessibleMacInterface : public nsIAccessibleMacInterface {
 public:
  // Construct an xpcAccessibleMacInterface using the native object
  // associated with this accessible or proxy.
  explicit xpcAccessibleMacInterface(AccessibleOrProxy aObj);

  // Construct an xpcAccessibleMacInterface using this
  // native object that conforms to the NSAccessibility protocol.
  explicit xpcAccessibleMacInterface(id aNativeObj);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEMACINTERFACE

 protected:
  virtual ~xpcAccessibleMacInterface();

  // Convert an NSObject (which can be anything, string, number, array, etc.)
  // into a properly typed js value populated in the aResult handle.
  nsresult NSObjectToJsValue(id aObj, JSContext* aCx,
                             JS::MutableHandleValue aResult);
  id mNativeObject;

 private:
  xpcAccessibleMacInterface(const xpcAccessibleMacInterface&) = delete;
  xpcAccessibleMacInterface& operator=(const xpcAccessibleMacInterface&) =
      delete;
};

}  // namespace a11y
}  // namespace mozilla

#endif
