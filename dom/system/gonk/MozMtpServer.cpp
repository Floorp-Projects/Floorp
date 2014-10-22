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
#include "DeviceStorage.h"
#include "mozilla/FileUtils.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "Volume.h"

#define DEFAULT_THREAD_TIMEOUT_MS 30000

using namespace android;
using namespace mozilla;
BEGIN_MTP_NAMESPACE

class FileWatcherUpdateRunnable MOZ_FINAL : public nsRunnable
{
public:
  FileWatcherUpdateRunnable(MozMtpDatabase* aMozMtpDatabase,
                            RefCountedMtpServer* aMtpServer,
                            DeviceStorageFile* aFile,
                            const nsACString& aEventType)
    : mMozMtpDatabase(aMozMtpDatabase),
      mMtpServer(aMtpServer),
      mFile(aFile),
      mEventType(aEventType)
  {}

  NS_IMETHOD Run()
  {
    // Runs on the FileWatcherUpdate->mIOThread
    MOZ_ASSERT(!NS_IsMainThread());

    mMozMtpDatabase->FileWatcherUpdate(mMtpServer, mFile, mEventType);
    return NS_OK;
  }

private:
  nsRefPtr<MozMtpDatabase> mMozMtpDatabase;
  nsRefPtr<RefCountedMtpServer> mMtpServer;
  nsRefPtr<DeviceStorageFile> mFile;
  nsCString mEventType;
};

// The FileWatcherUpdate class listens for file-watcher-update events
// and tells the MtpServer about the changes.
class FileWatcherUpdate MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  FileWatcherUpdate(MozMtpServer* aMozMtpServer)
    : mMozMtpServer(aMozMtpServer)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mIOThread = new LazyIdleThread(
      DEFAULT_THREAD_TIMEOUT_MS,
      NS_LITERAL_CSTRING("MTP FileWatcherUpdate"));

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, "file-watcher-update", false);
  }

  ~FileWatcherUpdate()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, "file-watcher-update");
  }

  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (strcmp(aTopic, "file-watcher-update")) {
      // We're only interested in file-watcher-update events
      return NS_OK;
    }

    NS_ConvertUTF16toUTF8 eventType(aData);
    if (!eventType.EqualsLiteral("modified") && !eventType.EqualsLiteral("deleted")) {
      // Bug 1074604: Needn't handle "created" event, once file operation
      // finished, it would trigger "modified" event.
      return NS_OK;
    }

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    file->Dump("file-watcher-update");
    MTP_LOG("file-watcher-update: file %s %s",
            NS_LossyConvertUTF16toASCII(file->mPath).get(),
            eventType.get());

    nsRefPtr<MozMtpDatabase> mozMtpDatabase = mMozMtpServer->GetMozMtpDatabase();
    nsRefPtr<RefCountedMtpServer> mtpServer = mMozMtpServer->GetMtpServer();

    // We're not supposed to perform I/O on the main thread, so punt the
    // notification (which will write to /dev/mtp_usb) to an I/O Thread.

    nsRefPtr<FileWatcherUpdateRunnable> r =
      new FileWatcherUpdateRunnable(mozMtpDatabase, mtpServer, file, eventType);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);

    return NS_OK;
  }

private:
  nsRefPtr<MozMtpServer> mMozMtpServer;
  nsCOMPtr<nsIThread> mIOThread;
};
NS_IMPL_ISUPPORTS(FileWatcherUpdate, nsIObserver)
static StaticRefPtr<FileWatcherUpdate> sFileWatcherUpdate;

class AllocFileWatcherUpdateRunnable MOZ_FINAL : public nsRunnable
{
public:
  AllocFileWatcherUpdateRunnable(MozMtpServer* aMozMtpServer)
    : mMozMtpServer(aMozMtpServer)
  {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    sFileWatcherUpdate = new FileWatcherUpdate(mMozMtpServer);
    return NS_OK;
  }
private:
  nsRefPtr<MozMtpServer> mMozMtpServer;
};

class FreeFileWatcherUpdateRunnable MOZ_FINAL : public nsRunnable
{
public:
  FreeFileWatcherUpdateRunnable(MozMtpServer* aMozMtpServer)
    : mMozMtpServer(aMozMtpServer)
  {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    sFileWatcherUpdate = nullptr;
    return NS_OK;
  }
private:
  nsRefPtr<MozMtpServer> mMozMtpServer;
};

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

    DebugOnly<nsresult> rv =
      NS_DispatchToMainThread(new AllocFileWatcherUpdateRunnable(mMozMtpServer));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    MTP_LOG("MozMtpServer started");
    server->run();
    MTP_LOG("MozMtpServer finished");

    // server->run will have closed the file descriptor.
    mMtpUsbFd.forget();

    rv = NS_DispatchToMainThread(new FreeFileWatcherUpdateRunnable(mMozMtpServer));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

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

bool
MozMtpServer::Init()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  const char *mtpUsbFilename = "/dev/mtp_usb";
  mMtpUsbFd = open(mtpUsbFilename, O_RDWR);
  if (mMtpUsbFd.get() < 0) {
    MTP_ERR("open of '%s' failed", mtpUsbFilename);
    return false;
  }
  MTP_LOG("Opened '%s' fd %d", mtpUsbFilename, mMtpUsbFd.get());

  mMozMtpDatabase = new MozMtpDatabase();
  mMtpServer = new RefCountedMtpServer(mMtpUsbFd.get(),        // fd
                                       mMozMtpDatabase.get(), // MtpDatabase
                                       false,                 // ptp?
                                       AID_MEDIA_RW,          // file group
                                       0664,                  // file permissions
                                       0775);                 // dir permissions
  return true;
}

void
MozMtpServer::Run()
{
  nsresult rv = NS_NewNamedThread("MtpServer", getter_AddRefs(mServerThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  MOZ_ASSERT(mServerThread);
  mServerThread->Dispatch(new MtpServerRunnable(mMtpUsbFd.forget(), this), NS_DISPATCH_NORMAL);
}

END_MTP_NAMESPACE
