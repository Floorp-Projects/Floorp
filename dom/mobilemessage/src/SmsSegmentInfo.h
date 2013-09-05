/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsSegmentInfo_h
#define mozilla_dom_mobilemessage_SmsSegmentInfo_h

#include "nsIDOMSmsSegmentInfo.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/mobilemessage/SmsTypes.h"

namespace mozilla {
namespace dom {

class SmsSegmentInfo MOZ_FINAL : public nsIDOMMozSmsSegmentInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZSMSSEGMENTINFO

  SmsSegmentInfo(int32_t aSegments,
                 int32_t aCharsPerSegment,
                 int32_t aCharsAvailableInLastSegment);

  SmsSegmentInfo(const mobilemessage::SmsSegmentInfoData& aData);

  const mobilemessage::SmsSegmentInfoData& GetData() const;

private:
  mobilemessage::SmsSegmentInfoData mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsSegmentInfo_h
