/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "LocalAccessible-inl.h"

#import "MUIAccessible.h"
#import "MUIRootAccessible.h"

using namespace mozilla::a11y;

//-----------------------------------------------------
// construction
//-----------------------------------------------------
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : LocalAccessible(aContent, aDoc),
      mNativeObject(nil),
      mNativeInited(false) {}

void AccessibleWrap::Shutdown() {
  // this ensures we will not try to re-create the native object.
  mNativeInited = true;

  // we really intend to access the member directly.
  if (mNativeObject) {
    [mNativeObject expire];
    [mNativeObject release];
    mNativeObject = nil;
  }

  LocalAccessible::Shutdown();
}

id AccessibleWrap::GetNativeObject() {
  if (!mNativeInited && !IsDefunct()) {
    Class type = IsRoot() ? [MUIRootAccessible class] : [MUIAccessible class];
    mNativeObject = [[type alloc] initWithAccessible:this];
  }

  mNativeInited = true;

  return mNativeObject;
}

void AccessibleWrap::GetNativeInterface(void** aOutInterface) {
  *aOutInterface = static_cast<void*>(GetNativeObject());
}
