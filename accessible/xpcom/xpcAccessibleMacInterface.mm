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
xpcAccessibleMacInterface::GetAttributeNames(nsTArray<nsString>& aAttributeNames) { return NS_OK; }

NS_IMETHODIMP
xpcAccessibleMacInterface::GetActionNames(nsTArray<nsString>& aActionNames) { return NS_OK; }

NS_IMETHODIMP
xpcAccessibleMacInterface::PerformAction(const nsAString& aActionName) { return NS_OK; }

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeValue(const nsAString& aAttributeName, JSContext* aCx,
                                             JS::MutableHandleValue aResult) {
  return NS_OK;
}
