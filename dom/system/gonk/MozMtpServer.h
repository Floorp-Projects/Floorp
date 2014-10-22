/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozmtpserver_h__
#define mozilla_system_mozmtpserver_h__

#include "MozMtpCommon.h"
#include "MozMtpDatabase.h"

#include "mozilla/FileUtils.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"

namespace mozilla {
namespace system {
  class Volume;
}
}

BEGIN_MTP_NAMESPACE
using namespace android;

class RefCountedMtpServer : public MtpServer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMtpServer)

  RefCountedMtpServer(int aFd, MtpDatabase* aDatabase, bool aPtp,
                      int aFileGroup, int aFilePerm, int aDirectoryPerm)
    : MtpServer(aFd, aDatabase, aPtp, aFileGroup, aFilePerm, aDirectoryPerm)
  {
  }
};

class MozMtpServer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MozMtpServer)

  bool Init();
  void Run();

  already_AddRefed<RefCountedMtpServer> GetMtpServer();
  already_AddRefed<MozMtpDatabase> GetMozMtpDatabase();

private:
  nsRefPtr<RefCountedMtpServer> mMtpServer;
  nsRefPtr<MozMtpDatabase> mMozMtpDatabase;
  nsCOMPtr<nsIThread> mServerThread;
  ScopedClose mMtpUsbFd;
};

END_MTP_NAMESPACE

#endif  // mozilla_system_mozmtpserver_h__


