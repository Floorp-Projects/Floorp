/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozMtpStorage.h"
#include "MozMtpDatabase.h"
#include "MozMtpServer.h"

#include "base/message_loop.h"
#include "nsXULAppAPI.h"

BEGIN_MTP_NAMESPACE
using namespace android;

MozMtpStorage::MozMtpStorage(Volume* aVolume, MozMtpServer* aMozMtpServer)
  : mMozMtpServer(aMozMtpServer),
    mVolume(aVolume)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // The MtpStorageID has the physical volume in the top 16 bits, and the
  // logical volumein the lower 16 bits. We treat each volume as a separate
  // phsyical storage;
  mStorageID = mVolume->Id() << 16 | 1;

  MTP_LOG("Storage constructed for Volume %s mStorageID 0x%08x",
          aVolume->NameStr(), mStorageID);

  Volume::RegisterVolumeObserver(this, "MozMtpStorage");

  // Get things in sync
  Notify(mVolume);
}

MozMtpStorage::~MozMtpStorage()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  MTP_LOG("Storage destructed for Volume %s mStorageID 0x%08x",
          mVolume->NameStr(), mStorageID);

  Volume::UnregisterVolumeObserver(this, "MozMtpStorage");
  if (mMtpStorage) {
    StorageUnavailable();
  }
}

// virtual
void
MozMtpStorage::Notify(Volume* const& aVolume)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (aVolume != mVolume) {
    // Not our volume
    return;
  }
  Volume::STATE volState = aVolume->State();

  MTP_LOG("Volume %s mStorageID 0x%08x state changed to %s SharingEnabled: %d",
          aVolume->NameStr(), mStorageID, aVolume->StateStr(),
          aVolume->IsSharingEnabled());

  // vol->IsSharingEnabled really only applies to UMS volumes. We assume that
  // that as long as MTP is enabled, then all volumes will be shared. The UI
  // currently doesn't give us anything more granular than on/off.

  if (mMtpStorage) {
    if (volState != nsIVolume::STATE_MOUNTED) {
      // The volume is no longer accessible. We need to remove this storage
      // from the MTP server
      StorageUnavailable();
    }
  } else {
    if (volState == nsIVolume::STATE_MOUNTED) {
      // The volume is accessible. Tell the MTP server.
      StorageAvailable();
    }
  }
}

void
MozMtpStorage::StorageAvailable()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  nsCString mountPoint = mVolume->MountPoint();

  MTP_LOG("Adding Volume %s mStorageID 0x%08x mountPoint %s to MozMtpDatabase",
          mVolume->NameStr(), mStorageID, mountPoint.get());

  RefPtr<MozMtpDatabase> db = mMozMtpServer->GetMozMtpDatabase();
  db->AddStorage(mStorageID, mountPoint.get(), mVolume->NameStr());

  MOZ_ASSERT(!mMtpStorage);

  //TODO: Figure out what to do about maxFileSize.

  mMtpStorage.reset(new MtpStorage(mStorageID,                           // id
                                   mountPoint.get(),                     // filePath
                                   mVolume->NameStr(),                   // description
                                   1024uLL * 1024uLL,                    // reserveSpace
                                   mVolume->IsHotSwappable(),            // removable
                                   2uLL * 1024uLL * 1024uLL * 1024uLL)); // maxFileSize
  RefPtr<RefCountedMtpServer> server = mMozMtpServer->GetMtpServer();

  MTP_LOG("Adding Volume %s mStorageID 0x%08x mountPoint %s to MtpServer",
          mVolume->NameStr(), mStorageID, mountPoint.get());
  server->addStorage(mMtpStorage.get());
}

void
MozMtpStorage::StorageUnavailable()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(mMtpStorage);

  MTP_LOG("Removing mStorageID 0x%08x from MtpServer", mStorageID);

  RefPtr<RefCountedMtpServer> server = mMozMtpServer->GetMtpServer();
  server->removeStorage(mMtpStorage.get());

  MTP_LOG("Removing mStorageID 0x%08x from MozMtpDatabse", mStorageID);

  RefPtr<MozMtpDatabase> db = mMozMtpServer->GetMozMtpDatabase();
  db->RemoveStorage(mStorageID);

  mMtpStorage = nullptr;
}

END_MTP_NAMESPACE


