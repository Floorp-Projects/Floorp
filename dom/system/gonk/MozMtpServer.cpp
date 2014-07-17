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

#include "mozilla/FileUtils.h"
#include "mozilla/Scoped.h"
#include "nsThreadUtils.h"

using namespace android;
using namespace mozilla;
USING_MTP_NAMESPACE

class MtpServerRunnable : public nsRunnable
{
public:
  nsresult Run()
  {
    const char *mtpUsbFilename = "/dev/mtp_usb";
    const char *productName = "FirefoxOS";
    const char *storageDir = "/storage/sdcard";

    mFd = open(mtpUsbFilename, O_RDWR);
    if (mFd.get() < 0) {
      MTP_LOG("open of '%s' failed", mtpUsbFilename);
      return NS_OK;
    }

    MTP_LOG("MozMtpServer open done, fd: %d. Start reading.", mFd.get());

    ScopedDeletePtr<MozMtpDatabase> database;
    ScopedDeletePtr<MtpServer> server;
    ScopedDeletePtr<MtpStorage> storage;

    database = new MozMtpDatabase(storageDir);
    server = new MtpServer(mFd.get(), database, false, 1023, 0664, 0775);
    storage = new MtpStorage(MTP_STORAGE_FIXED_RAM,       // id
                             storageDir,                  // filePath
                             productName,                 // description
                             100uLL * 1024uLL * 1024uLL,  // reserveSpace
                             false,                       // removable
                             2uLL * 1024uLL * 1024uLL * 1024uLL); // maxFileSize

    server->addStorage(storage);

    MTP_LOG("MozMtpServer started");
    server->run();
    MTP_LOG("MozMtpServer finished");

    return NS_OK;
  }

private:
  ScopedClose mFd;
};

void
MozMtpServer::Run()
{
  nsresult rv = NS_NewNamedThread("MtpServer", getter_AddRefs(mServerThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  MOZ_ASSERT(mServerThread);
  mServerThread->Dispatch(new MtpServerRunnable(), 0);
}
