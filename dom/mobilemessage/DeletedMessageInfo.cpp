/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeletedMessageInfo.h"
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsVariant.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(DeletedMessageInfo, nsIDeletedMessageInfo)

DeletedMessageInfo::DeletedMessageInfo(const DeletedMessageInfoData& aData)
  : mData(aData)
{
}

DeletedMessageInfo::DeletedMessageInfo(int32_t* aMessageIds,
                                       uint32_t aMsgCount,
                                       uint64_t* aThreadIds,
                                       uint32_t  aThreadCount)
{
  mData.deletedMessageIds().AppendElements(aMessageIds, aMsgCount);
  mData.deletedThreadIds().AppendElements(aThreadIds, aThreadCount);
}

DeletedMessageInfo::~DeletedMessageInfo()
{
}

/* static */ nsresult
DeletedMessageInfo::Create(int32_t* aMessageIds,
                           uint32_t aMsgCount,
                           uint64_t* aThreadIds,
                           uint32_t  aThreadCount,
                           nsIDeletedMessageInfo** aDeletedInfo)
{
  NS_ENSURE_ARG_POINTER(aDeletedInfo);
  NS_ENSURE_TRUE(aMsgCount || aThreadCount, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDeletedMessageInfo> deletedInfo =
    new DeletedMessageInfo(aMessageIds,
                           aMsgCount,
                           aThreadIds,
                           aThreadCount);
  deletedInfo.forget(aDeletedInfo);

  return NS_OK;
}

NS_IMETHODIMP
DeletedMessageInfo::GetDeletedMessageIds(nsIVariant** aDeletedMessageIds)
{
  NS_ENSURE_ARG_POINTER(aDeletedMessageIds);

  if (mDeletedMessageIds) {
    NS_ADDREF(*aDeletedMessageIds = mDeletedMessageIds);
    return NS_OK;
  }

  uint32_t length = mData.deletedMessageIds().Length();

  if (length == 0) {
    *aDeletedMessageIds = nullptr;
    return NS_OK;
  }

  mDeletedMessageIds = new nsVariant();

  nsresult rv;
  rv = mDeletedMessageIds->SetAsArray(nsIDataType::VTYPE_INT32,
                                      nullptr,
                                      length,
                                      mData.deletedMessageIds().Elements());
  NS_ENSURE_SUCCESS(rv, rv);

  mDeletedMessageIds->SetWritable(false);

  NS_ADDREF(*aDeletedMessageIds = mDeletedMessageIds);

  return NS_OK;
}

NS_IMETHODIMP
DeletedMessageInfo::GetDeletedThreadIds(nsIVariant** aDeletedThreadIds)
{
  NS_ENSURE_ARG_POINTER(aDeletedThreadIds);

  if (mDeletedThreadIds) {
    NS_ADDREF(*aDeletedThreadIds = mDeletedThreadIds);
    return NS_OK;
  }

  uint32_t length = mData.deletedThreadIds().Length();

  if (length == 0) {
    *aDeletedThreadIds = nullptr;
    return NS_OK;
  }

  mDeletedThreadIds = new nsVariant();

  nsresult rv;
  rv = mDeletedThreadIds->SetAsArray(nsIDataType::VTYPE_UINT64,
                                     nullptr,
                                     length,
                                     mData.deletedThreadIds().Elements());
  NS_ENSURE_SUCCESS(rv, rv);

  mDeletedThreadIds->SetWritable(false);

  NS_ADDREF(*aDeletedThreadIds = mDeletedThreadIds);

  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
