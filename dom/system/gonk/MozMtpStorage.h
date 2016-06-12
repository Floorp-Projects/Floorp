/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozmtpstorage_h__
#define mozilla_system_mozmtpstorage_h__

#include "MozMtpCommon.h"

#include "mozilla/UniquePtr.h"

#include "Volume.h"

BEGIN_MTP_NAMESPACE
using namespace android;

class MozMtpServer;

class MozMtpStorage : public Volume::EventObserver
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MozMtpStorage)

  MozMtpStorage(Volume* aVolume, MozMtpServer* aMozMtpServer);

  typedef nsTArray<RefPtr<MozMtpStorage> > Array;

private:
  virtual ~MozMtpStorage();
  virtual void Notify(Volume* const& aEvent);

  void StorageAvailable();
  void StorageUnavailable();

  RefPtr<MozMtpServer>  mMozMtpServer;
  UniquePtr<MtpStorage>   mMtpStorage;
  RefPtr<Volume>          mVolume;
  MtpStorageID            mStorageID;
};

END_MTP_NAMESPACE

#endif  // mozilla_system_mozmtpstorage_h__


