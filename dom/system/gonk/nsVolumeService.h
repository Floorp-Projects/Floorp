/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolumeservice_h__
#define mozilla_system_nsvolumeservice_h__

#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsVolume.h"

namespace mozilla {

namespace dom {
class VolumeInfo;
} // dom

namespace system {

class Volume;

/***************************************************************************
* The nsVolumeData class encapsulates the data that is updated/maintained
* on the main thread in order to support the nsIVolume and nsIVolumeService
* classes.
*/

class nsVolumeService final : public nsIVolumeService,
                              public nsIDOMMozWakeLockListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVOLUMESERVICE
  NS_DECL_NSIDOMMOZWAKELOCKLISTENER

  nsVolumeService();

  static already_AddRefed<nsVolumeService> GetSingleton();
  //static nsVolumeService* GetSingleton();
  static void Shutdown();

  void DumpNoLock(const char* aLabel);

  // To use this function, you have to create a new volume and pass it in.
  void UpdateVolume(nsVolume* aVolume, bool aNotifyObservers = true);
  void UpdateVolumeIOThread(const Volume* aVolume);

  void RecvVolumesFromParent(const nsTArray<dom::VolumeInfo>& aVolumes);
  void GetVolumesForIPC(nsTArray<dom::VolumeInfo>* aResult);

  void RemoveVolumeByName(const nsAString& aName);

private:
  ~nsVolumeService();

  void CheckMountLock(const nsAString& aMountLockName,
                      const nsAString& aMountLockState);
  already_AddRefed<nsVolume> FindVolumeByMountLockName(const nsAString& aMountLockName);

  already_AddRefed<nsVolume> FindVolumeByName(const nsAString& aName,
                                              nsVolume::Array::index_type* aIndex = nullptr);

  Monitor mArrayMonitor;
  nsVolume::Array mVolumeArray;

  static StaticRefPtr<nsVolumeService> sSingleton;
  bool mGotVolumesFromParent;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolumeservice_h__
