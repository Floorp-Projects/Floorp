/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcMessageHandler.h"

#include <binder/Parcel.h>

using namespace android;
using namespace mozilla;

bool
NfcMessageHandler::Marshall(Parcel& aParcel, const CommandOptions& aOptions)
{
  bool result;
  // TODO: Implementation will be Bug 933588 - Part 2.
  return result;
}

bool
NfcMessageHandler::Unmarshall(const Parcel& aParcel, EventOptions& aOptions)
{
  bool result;
  // TODO: Implementation will be Bug 933588 - Part 2.
  return result;
}

