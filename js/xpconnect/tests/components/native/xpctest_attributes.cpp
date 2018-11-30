/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpctest_private.h"

NS_IMPL_ISUPPORTS(xpcTestObjectReadOnly, nsIXPCTestObjectReadOnly)

xpcTestObjectReadOnly ::xpcTestObjectReadOnly() {
  boolProperty = true;
  shortProperty = 32767;
  longProperty = 2147483647;
  floatProperty = 5.5f;
  charProperty = 'X';
  // timeProperty is PRTime and signed type.
  // So it has to allow negative value.
  timeProperty = -1;
}

NS_IMETHODIMP xpcTestObjectReadOnly ::GetStrReadOnly(char** aStrReadOnly) {
  if (!aStrReadOnly) return NS_ERROR_NULL_POINTER;
  *aStrReadOnly = moz_xstrdup("XPConnect Read-Only String");
  return NS_OK;
}

NS_IMETHODIMP xpcTestObjectReadOnly ::GetBoolReadOnly(bool* aBoolReadOnly) {
  *aBoolReadOnly = boolProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly ::GetShortReadOnly(
    int16_t* aShortReadOnly) {
  *aShortReadOnly = shortProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly ::GetLongReadOnly(int32_t* aLongReadOnly) {
  *aLongReadOnly = longProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly ::GetFloatReadOnly(float* aFloatReadOnly) {
  *aFloatReadOnly = floatProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly ::GetCharReadOnly(char* aCharReadOnly) {
  *aCharReadOnly = charProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly ::GetTimeReadOnly(PRTime* aTimeReadOnly) {
  *aTimeReadOnly = timeProperty;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(xpcTestObjectReadWrite, nsIXPCTestObjectReadWrite)

xpcTestObjectReadWrite ::xpcTestObjectReadWrite() {
  stringProperty = moz_xstrdup("XPConnect Read-Writable String");
  boolProperty = true;
  shortProperty = 32767;
  longProperty = 2147483647;
  floatProperty = 5.5f;
  charProperty = 'X';
  // timeProperty is PRTime and signed type.
  // So it has to allow negative value.
  timeProperty = -1;
}

xpcTestObjectReadWrite ::~xpcTestObjectReadWrite() { free(stringProperty); }

NS_IMETHODIMP xpcTestObjectReadWrite ::GetStringProperty(
    char** aStringProperty) {
  if (!aStringProperty) {
    return NS_ERROR_NULL_POINTER;
  }
  *aStringProperty = moz_xstrdup(stringProperty);
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetStringProperty(
    const char* aStringProperty) {
  free(stringProperty);
  stringProperty = moz_xstrdup(aStringProperty);
  return NS_OK;
}

NS_IMETHODIMP xpcTestObjectReadWrite ::GetBooleanProperty(
    bool* aBooleanProperty) {
  *aBooleanProperty = boolProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetBooleanProperty(
    bool aBooleanProperty) {
  boolProperty = aBooleanProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::GetShortProperty(
    int16_t* aShortProperty) {
  *aShortProperty = shortProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetShortProperty(
    int16_t aShortProperty) {
  shortProperty = aShortProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::GetLongProperty(int32_t* aLongProperty) {
  *aLongProperty = longProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetLongProperty(int32_t aLongProperty) {
  longProperty = aLongProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::GetFloatProperty(float* aFloatProperty) {
  *aFloatProperty = floatProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetFloatProperty(float aFloatProperty) {
  floatProperty = aFloatProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::GetCharProperty(char* aCharProperty) {
  *aCharProperty = charProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetCharProperty(char aCharProperty) {
  charProperty = aCharProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::GetTimeProperty(PRTime* aTimeProperty) {
  *aTimeProperty = timeProperty;
  return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite ::SetTimeProperty(PRTime aTimeProperty) {
  timeProperty = aTimeProperty;
  return NS_OK;
}
