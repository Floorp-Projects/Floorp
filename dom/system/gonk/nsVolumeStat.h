/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolumestat_h__
#define mozilla_system_nsvolumestat_h__

#include "nsIVolumeStat.h"
#include "nsString.h"
#include <sys/statfs.h>

namespace mozilla {
namespace system {

class nsVolumeStat final : public nsIVolumeStat
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOLUMESTAT

  nsVolumeStat(const nsAString& aPath);

protected:
  ~nsVolumeStat() {}

private:
  struct statfs mStat;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolumestat_h__
