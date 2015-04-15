/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// Conversion
//

nsresult
Convert(nsresult aIn, BluetoothStatus& aOut)
{
  if (NS_SUCCEEDED(aIn)) {
    aOut = STATUS_SUCCESS;
  } else if (aIn == NS_ERROR_OUT_OF_MEMORY) {
    aOut = STATUS_NOMEM;
  } else {
    aOut = STATUS_FAIL;
  }
  return NS_OK;
}

END_BLUETOOTH_NAMESPACE
