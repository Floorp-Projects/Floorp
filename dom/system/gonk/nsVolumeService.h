/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolumeservice_h__
#define mozilla_system_nsvolumeservice_h__

#include "nsCOMPtr.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsVolume.h"
#include "Volume.h"

namespace mozilla {
namespace system {

/***************************************************************************
* The nsVolumeData class encapsulates the data that is updated/maintained
* on the main thread in order to support the nsIVolume and nsIVolumeService
* classes.
*/

class nsVolumeService : public nsIVolumeService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOLUMESERVICE

  nsVolumeService();

  already_AddRefed<nsVolume> FindVolumeByName(const nsAString &aName);
  already_AddRefed<nsVolume> FindAddVolumeByName(const nsAString &aName);
  void UpdateVolume(const nsVolume *aVolume);
  static void UpdateVolumeIOThread(const Volume *aVolume);

private:
  ~nsVolumeService();

  nsVolume::Array  mVolumeArray;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolumeservice_h__
