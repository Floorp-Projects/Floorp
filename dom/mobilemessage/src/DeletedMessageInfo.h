/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_DeletedMessageInfo_h
#define mozilla_dom_mobilemessage_DeletedMessageInfo_h

#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsIDeletedMessageInfo.h"

class nsIWritableVariant;

namespace mozilla {
namespace dom {
namespace mobilemessage {

class DeletedMessageInfo MOZ_FINAL : public nsIDeletedMessageInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDELETEDMESSAGEINFO

  DeletedMessageInfo(const DeletedMessageInfoData& aData);

  DeletedMessageInfo(int32_t* aMessageIds,
                     uint32_t aMsgCount,
                     uint64_t* aThreadIds,
                     uint32_t  aThreadCount);

  static nsresult Create(int32_t* aMessageIds,
                         uint32_t aMsgCount,
                         uint64_t* aThreadIds,
                         uint32_t  aThreadCount,
                         nsIDeletedMessageInfo** aDeletedInfo);

  const DeletedMessageInfoData& GetData() const { return mData; }

private:
  // Don't try to use the default constructor.
  DeletedMessageInfo();

  DeletedMessageInfoData mData;

  nsCOMPtr<nsIWritableVariant> mDeletedMessageIds;
  nsCOMPtr<nsIWritableVariant> mDeletedThreadIds;

protected:
  /* additional members */
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_DeletedMessageInfo_h
