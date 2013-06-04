/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolumeStat.h"
#include "nsString.h"

namespace mozilla {
namespace system {

NS_IMPL_ISUPPORTS1(nsVolumeStat, nsIVolumeStat)

nsVolumeStat::nsVolumeStat(const nsAString& aPath)
{
  if (statfs(NS_ConvertUTF16toUTF8(aPath).get(), &mStat) != 0) {
    memset(&mStat, 0, sizeof(mStat));
  }
}

nsVolumeStat::~nsVolumeStat()
{
}

/* readonly attribute long long totalBytes; */
NS_IMETHODIMP nsVolumeStat::GetTotalBytes(int64_t* aTotalBytes)
{
  *aTotalBytes = mStat.f_blocks * mStat.f_bsize;
  return NS_OK;
}

/* readonly attribute long long freeBytes; */
NS_IMETHODIMP nsVolumeStat::GetFreeBytes(int64_t* aFreeBytes)
{
  *aFreeBytes = mStat.f_bfree * mStat.f_bsize;
  return NS_OK;
}

} // system
} // mozilla
