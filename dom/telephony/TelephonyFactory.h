/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyFactory_h
#define mozilla_dom_telephony_TelephonyFactory_h

#include "nsCOMPtr.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

class nsITelephonyProvider;

BEGIN_TELEPHONY_NAMESPACE

class TelephonyFactory
{
public:
  static already_AddRefed<nsITelephonyProvider> CreateTelephonyProvider();
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_TelephonyFactory_h
