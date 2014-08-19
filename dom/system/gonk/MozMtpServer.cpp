/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozMtpServer.h"
#include "MozMtpDatabase.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Scoped.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "Volume.h"

using namespace android;
using namespace mozilla;
BEGIN_MTP_NAMESPACE

class MtpServerRunnable : public nsRunnable
{
public:
  MtpServerRunnable(int aMtpUsbFd, MozMtpServer* aMozMtpServer)
    : mMozMtpServer(aMozMtpServer),
      mMtpUsbFd(aMtpUsbFd)
  {
  }

  nsresult Run()
  {
    nsRefPtr<RefCountedMtpServer> server = mMozMtpServer->GetMtpServer();

    MTP_LOG("MozMtpServer started");
    server->run();
    MTP_LOG("MozMtpServer finished");

    return NS_OK;
  }

private:
  nsRefPtr<MozMtpServer> mMozMtpServer;
  ScopedClose mMtpUsbFd; // We want to hold this open while the server runs
};

already_AddRefed<RefCountedMtpServer>
MozMtpServer::GetMtpServer()
{
  nsRefPtr<RefCountedMtpServer> server = mMtpServer;
  return server.forget();
}

already_AddRefed<MozMtpDatabase>
MozMtpServer::GetMozMtpDatabase()
{
  nsRefPtr<MozMtpDatabase> db = mMozMtpDatabase;
  return db.forget();
}

void
MozMtpServer::Run()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  const char *mtpUsbFilename = "/dev/mtp_usb";
  ScopedClose mtpUsbFd(open(mtpUsbFilename, O_RDWR));
  if (mtpUsbFd.get() < 0) {
    MTP_ERR("open of '%s' failed", mtpUsbFilename);
    return;
  }
  MTP_LOG("Opened '%s' fd %d", mtpUsbFilename, mtpUsbFd.get());

  mMozMtpDatabase = new MozMtpDatabase();
  mMtpServer = new RefCountedMtpServer(mtpUsbFd.get(),        // fd
                                       mMozMtpDatabase.get(), // MtpDatabase
                                       false,                 // ptp?
                                       AID_MEDIA_RW,          // file group
                                       0664,                  // file permissions
                                       0775);                 // dir permissions

  nsresult rv = NS_NewNamedThread("MtpServer", getter_AddRefs(mServerThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  MOZ_ASSERT(mServerThread);
  mServerThread->Dispatch(new MtpServerRunnable(mtpUsbFd.forget(), this), NS_DISPATCH_NORMAL);
}

END_MTP_NAMESPACE
