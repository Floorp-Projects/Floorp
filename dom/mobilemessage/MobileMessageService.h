/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageService_h
#define mozilla_dom_mobilemessage_MobileMessageService_h

#include "mozilla/Attributes.h" // For final
#include "nsIMobileMessageService.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class MobileMessageService final : public nsIMobileMessageService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGESERVICE

  MobileMessageService() { MOZ_COUNT_CTOR(MobileMessageService); }

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~MobileMessageService() { MOZ_COUNT_DTOR(MobileMessageService); }
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageService_h
