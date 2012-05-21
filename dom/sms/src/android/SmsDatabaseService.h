/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsDatabaseService_h
#define mozilla_dom_sms_SmsDatabaseService_h

#include "nsISmsDatabaseService.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsDatabaseService : public nsISmsDatabaseService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSDATABASESERVICE
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsDatabaseService_h
