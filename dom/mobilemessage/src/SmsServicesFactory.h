/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsServicesFactory_h
#define mozilla_dom_mobilemessage_SmsServicesFactory_h

#include "nsCOMPtr.h"

class nsISmsService;
class nsIMmsService;
class nsIMobileMessageDatabaseService;

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsServicesFactory
{
public:
  static already_AddRefed<nsISmsService> CreateSmsService();
  static already_AddRefed<nsIMobileMessageDatabaseService> CreateMobileMessageDatabaseService();
  static already_AddRefed<nsIMmsService> CreateMmsService();
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsServicesFactory_h
