/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsIPCService_h
#define mozilla_dom_mobilemessage_SmsIPCService_h

#include "nsISmsService.h"
#include "nsIMmsService.h"
#include "nsIMobileMessageDatabaseService.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsIPCService final : public nsISmsService
                          , public nsIMmsService
                          , public nsIMobileMessageDatabaseService
                          , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSSERVICE
  NS_DECL_NSIMMSSERVICE
  NS_DECL_NSIMOBILEMESSAGEDATABASESERVICE
  NS_DECL_NSIOBSERVER

  static already_AddRefed<SmsIPCService>
  GetSingleton();

private:
  SmsIPCService();

  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~SmsIPCService();

  uint32_t mMmsDefaultServiceId;
  uint32_t mSmsDefaultServiceId;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsIPCService_h
