/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsIPCService_h
#define mozilla_dom_sms_SmsIPCService_h

#include "nsISmsService.h"
#include "nsISmsDatabaseService.h"

namespace mozilla {
namespace dom {
namespace sms {

class PSmsChild;

class SmsIPCService : public nsISmsService
                    , public nsISmsDatabaseService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSSERVICE
  NS_DECL_NSISMSDATABASESERVICE

private:
  static PSmsChild* GetSmsChild();
  static PSmsChild* sSmsChild;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsIPCService_h
