/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <errno.h>
#include "nsIObserverService.h"
#include "nsIDiskSpaceWatcher.h"
#include "mozilla/ModuleUtils.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "base/message_loop.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsXULAppAPI.h"
#include "fanotify.h"
#include "DiskSpaceWatcher.h"

using namespace mozilla;

namespace mozilla { namespace hal_impl { class GonkDiskSpaceWatcher; } }

using namespace mozilla::hal_impl;

template<>
struct RunnableMethodTraits<GonkDiskSpaceWatcher>
{
  static void RetainCallee(GonkDiskSpaceWatcher* obj) { }
  static void ReleaseCallee(GonkDiskSpaceWatcher* obj) { }
};

namespace mozilla {
namespace hal_impl {

// fanotify_init and fanotify_mark functions are syscalls.
// The user space bits are not part of bionic so we add them here
// as well as fanotify.h
int fanotify_init (unsigned int flags, unsigned int event_f_flags)
{
  return syscall(367, flags, event_f_flags);
}

// Add, remove, or modify an fanotify mark on a filesystem object.
int fanotify_mark (int fanotify_fd, unsigned int flags,
                   uint64_t mask, int dfd, const char *pathname)
{

  // On 32 bits platforms we have to convert the 64 bits mask into
  // two 32 bits ints.
  if (sizeof(void *) == 4) {
    union {
      uint64_t _64;
      uint32_t _32[2];
    } _mask;
    _mask._64 = mask;
    return syscall(368, fanotify_fd, flags, _mask._32[0], _mask._32[1],
                   dfd, pathname);
  }

  return syscall(368, fanotify_fd, flags, mask, dfd, pathname);
}

class GonkDiskSpaceWatcher MOZ_FINAL : public MessageLoopForIO::Watcher
{
public:
  GonkDiskSpaceWatcher();
  ~GonkDiskSpaceWatcher() {};

  virtual void OnFileCanReadWithoutBlocking(int aFd);

  // We should never write to the fanotify fd.
  virtual void OnFileCanWriteWithoutBlocking(int aFd)
  {
    MOZ_CRASH("Must not write to fanotify fd");
    MOZ_CRASH();
  }

  void DoStart();
  void DoStop();

private:
  void NotifyUpdate();

  uint64_t mLowThreshold;
  uint64_t mHighThreshold;
  TimeDuration mTimeout;
  TimeStamp  mLastTimestamp;
  uint64_t mLastFreeSpace;
  uint32_t mSizeDelta;

  bool mIsDiskFull;
  uint64_t mFreeSpace;

  int mFd;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
};

static GonkDiskSpaceWatcher* gHalDiskSpaceWatcher = nullptr;

#define WATCHER_PREF_LOW        "disk_space_watcher.low_threshold"
#define WATCHER_PREF_HIGH       "disk_space_watcher.high_threshold"
#define WATCHER_PREF_TIMEOUT    "disk_space_watcher.timeout"
#define WATCHER_PREF_SIZE_DELTA "disk_space_watcher.size_delta"

static const char kWatchedPath[] = "/data";

// Helper class to dispatch calls to xpcom on the main thread.
class DiskSpaceNotifier : public nsRunnable
{
public:
  DiskSpaceNotifier(const bool aIsDiskFull, const uint64_t aFreeSpace) :
    mIsDiskFull(aIsDiskFull),
    mFreeSpace(aFreeSpace) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    DiskSpaceWatcher::UpdateState(mIsDiskFull, mFreeSpace);
    return NS_OK;
  }

private:
  bool mIsDiskFull;
  uint64_t mFreeSpace;
};

// Helper runnable to delete the watcher on the main thread.
class DiskSpaceCleaner : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    delete gHalDiskSpaceWatcher;
    return NS_OK;
  }
};

GonkDiskSpaceWatcher::GonkDiskSpaceWatcher() :
  mLastFreeSpace(UINT64_MAX),
  mIsDiskFull(false),
  mFreeSpace(UINT64_MAX),
  mFd(-1)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gHalDiskSpaceWatcher == nullptr);

  // Default values: 5MB for low threshold, 10MB for high threshold, and
  // a timeout of 5 seconds.
  mLowThreshold = Preferences::GetInt(WATCHER_PREF_LOW, 5) * 1024 * 1024;
  mHighThreshold = Preferences::GetInt(WATCHER_PREF_HIGH, 10) * 1024 * 1024;
  mTimeout = TimeDuration::FromSeconds(Preferences::GetInt(WATCHER_PREF_TIMEOUT, 5));
  mSizeDelta = Preferences::GetInt(WATCHER_PREF_SIZE_DELTA, 1) * 1024 * 1024;
}

void
GonkDiskSpaceWatcher::DoStart()
{
  NS_ASSERTION(XRE_GetIOMessageLoop() == MessageLoopForIO::current(),
               "Not on the correct message loop");

  mFd = fanotify_init(FAN_CLASS_NOTIF, FAN_CLOEXEC);
  if (mFd == -1) {
    NS_WARNING("Error calling inotify_init()");
    if (errno == ENOSYS) {
      printf_stderr("Warning: No fanotify support in this device's kernel.\n");
    }
    return;
  }

  if (fanotify_mark(mFd, FAN_MARK_ADD | FAN_MARK_MOUNT, FAN_CLOSE,
                    0, kWatchedPath) < 0) {
    NS_WARNING("Error calling fanotify_mark");
    close(mFd);
    mFd = -1;
    return;
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
    mFd, /* persistent = */ true,
    MessageLoopForIO::WATCH_READ,
    &mReadWatcher, gHalDiskSpaceWatcher)) {
      NS_WARNING("Unable to watch fanotify fd.");
      close(mFd);
      mFd = -1;
  }
}

void
GonkDiskSpaceWatcher::DoStop()
{
  NS_ASSERTION(XRE_GetIOMessageLoop() == MessageLoopForIO::current(),
               "Not on the correct message loop");

  if (mFd != -1) {
    mReadWatcher.StopWatchingFileDescriptor();
    fanotify_mark(mFd, FAN_MARK_FLUSH, 0, 0, kWatchedPath);
    close(mFd);
    mFd = -1;
  }

  // Dispatch the cleanup to the main thread.
  nsCOMPtr<nsIRunnable> runnable = new DiskSpaceCleaner();
  NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
}

// We are called off the main thread, so we proxy first to the main thread
// before calling the xpcom object.
void
GonkDiskSpaceWatcher::NotifyUpdate()
{
  mLastTimestamp = TimeStamp::Now();
  mLastFreeSpace = mFreeSpace;

  nsCOMPtr<nsIRunnable> runnable =
    new DiskSpaceNotifier(mIsDiskFull, mFreeSpace);
  NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
}

void
GonkDiskSpaceWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  struct fanotify_event_metadata* fem = nullptr;
  char buf[4096];
  struct statfs sfs;
  int32_t len, rc;

  do {
    len = read(aFd, buf, sizeof(buf));
  } while(len == -1 && errno == EINTR);

  // Bail out if the file is busy.
  if (len < 0 && errno == ETXTBSY) {
    return;
  }

  // We should get an exact multiple of fanotify_event_metadata
  if (len <= 0 || (len % FAN_EVENT_METADATA_LEN != 0)) {
    printf_stderr("About to crash: fanotify_event_metadata read error.");
    MOZ_CRASH();
  }

  fem = reinterpret_cast<fanotify_event_metadata *>(buf);

  while (FAN_EVENT_OK(fem, len)) {
    rc = fstatfs(fem->fd, &sfs);
    if (rc < 0) {
      NS_WARNING("Unable to stat fan_notify fd");
    } else {
      bool firstRun = mFreeSpace == UINT64_MAX;
      mFreeSpace = sfs.f_bavail * sfs.f_bsize;
      // We change from full <-> free depending on the free space and the
      // low and high thresholds.
      // Once we are in 'full' mode we send updates for all size changes with
      // a minimum of time between messages or when we cross a size change
      // threshold.
      if (firstRun) {
        mIsDiskFull = mFreeSpace <= mLowThreshold;
        // Always notify the current state at first run.
        NotifyUpdate();
      } else if (!mIsDiskFull && (mFreeSpace <= mLowThreshold)) {
        mIsDiskFull = true;
        NotifyUpdate();
      } else if (mIsDiskFull && (mFreeSpace > mHighThreshold)) {
        mIsDiskFull = false;
        NotifyUpdate();
      } else if (mIsDiskFull) {
        if (mTimeout < TimeStamp::Now() - mLastTimestamp ||
            mSizeDelta < llabs(mFreeSpace - mLastFreeSpace)) {
          NotifyUpdate();
        }
      }
    }
    close(fem->fd);
    fem = FAN_EVENT_NEXT(fem, len);
  }
}

void
StartDiskSpaceWatcher()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Bail out if called several times.
  if (gHalDiskSpaceWatcher != nullptr) {
    return;
  }

  gHalDiskSpaceWatcher = new GonkDiskSpaceWatcher();

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(gHalDiskSpaceWatcher, &GonkDiskSpaceWatcher::DoStart));
}

void
StopDiskSpaceWatcher()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!gHalDiskSpaceWatcher) {
    return;
  }

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(gHalDiskSpaceWatcher, &GonkDiskSpaceWatcher::DoStop));
}

} // namespace hal_impl
} // namespace mozilla
