/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsServicesFactory_h
#define mozilla_dom_sms_SmsServicesFactory_h

#include "nsCOMPtr.h"

class nsISmsService;
class nsISmsDatabaseService;

namespace mozilla {
namespace dom {
namespace sms {

class SmsServicesFactory
{
public:
  static already_AddRefed<nsISmsService> CreateSmsService();
  static already_AddRefed<nsISmsDatabaseService> CreateSmsDatabaseService();
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsServicesFactory_h
