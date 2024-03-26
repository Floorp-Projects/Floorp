/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsPrintfCString.h"

#include "XPCSelfHostedShmem.h"

namespace mozilla {
namespace ipc {

SharedPreferenceSerializer::SharedPreferenceSerializer()
    : mPrefMapSize(0), mPrefsLength(0) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::~SharedPreferenceSerializer() {
  MOZ_COUNT_DTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::SharedPreferenceSerializer(
    SharedPreferenceSerializer&& aOther)
    : mPrefMapSize(aOther.mPrefMapSize),
      mPrefsLength(aOther.mPrefsLength),
      mPrefMapHandle(std::move(aOther.mPrefMapHandle)),
      mPrefsHandle(std::move(aOther.mPrefsHandle)) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

bool SharedPreferenceSerializer::SerializeToSharedMemory(
    const GeckoProcessType aDestinationProcessType,
    const nsACString& aDestinationRemoteType) {
  mPrefMapHandle =
      Preferences::EnsureSnapshot(&mPrefMapSize).TakePlatformHandle();

  bool destIsWebContent =
      aDestinationProcessType == GeckoProcessType_Content &&
      (StringBeginsWith(aDestinationRemoteType, WEB_REMOTE_TYPE) ||
       StringBeginsWith(aDestinationRemoteType, PREALLOC_REMOTE_TYPE));

  // Serialize the early prefs.
  nsAutoCStringN<1024> prefs;
  Preferences::SerializePreferences(prefs, destIsWebContent);
  mPrefsLength = prefs.Length();

  base::SharedMemory shm;
  // Set up the shared memory.
  if (!shm.Create(prefs.Length())) {
    NS_ERROR("failed to create shared memory in the parent");
    return false;
  }
  if (!shm.Map(prefs.Length())) {
    NS_ERROR("failed to map shared memory in the parent");
    return false;
  }

  // Copy the serialized prefs into the shared memory.
  memcpy(static_cast<char*>(shm.memory()), prefs.get(), mPrefsLength);

  mPrefsHandle = shm.TakeHandle();
  return true;
}

void SharedPreferenceSerializer::AddSharedPrefCmdLineArgs(
    mozilla::ipc::GeckoChildProcessHost& procHost,
    std::vector<std::string>& aExtraOpts) const {
#if defined(XP_WIN)
  // Record the handle as to-be-shared, and pass it via a command flag. This
  // works because Windows handles are system-wide.
  procHost.AddHandleToShare(GetPrefsHandle().get());
  procHost.AddHandleToShare(GetPrefMapHandle().get());
  geckoargs::sPrefsHandle.Put((uintptr_t)(GetPrefsHandle().get()), aExtraOpts);
  geckoargs::sPrefMapHandle.Put((uintptr_t)(GetPrefMapHandle().get()),
                                aExtraOpts);
#else
  // In contrast, Unix fds are per-process. So remap the fd to a fixed one that
  // will be used in the child.
  // XXX: bug 1440207 is about improving how fixed fds are used.
  //
  // Note: on Android, AddFdToRemap() sets up the fd to be passed via a Parcel,
  // and the fixed fd isn't used. However, we still need to mark it for
  // remapping so it doesn't get closed in the child.
  procHost.AddFdToRemap(GetPrefsHandle().get(), kPrefsFileDescriptor);
  procHost.AddFdToRemap(GetPrefMapHandle().get(), kPrefMapFileDescriptor);
#endif

  // Pass the lengths via command line flags.
  geckoargs::sPrefsLen.Put((uintptr_t)(GetPrefsLength()), aExtraOpts);
  geckoargs::sPrefMapSize.Put((uintptr_t)(GetPrefMapSize()), aExtraOpts);
}

#if defined(ANDROID) || defined(XP_IOS)
static int gPrefsFd = -1;
static int gPrefMapFd = -1;

void SetPrefsFd(int aFd) { gPrefsFd = aFd; }

void SetPrefMapFd(int aFd) { gPrefMapFd = aFd; }
#endif

SharedPreferenceDeserializer::SharedPreferenceDeserializer() {
  MOZ_COUNT_CTOR(SharedPreferenceDeserializer);
}

SharedPreferenceDeserializer::~SharedPreferenceDeserializer() {
  MOZ_COUNT_DTOR(SharedPreferenceDeserializer);
}

bool SharedPreferenceDeserializer::DeserializeFromSharedMemory(
    uint64_t aPrefsHandle, uint64_t aPrefMapHandle, uint64_t aPrefsLen,
    uint64_t aPrefMapSize) {
  Maybe<base::SharedMemoryHandle> prefsHandle;

#ifdef XP_WIN
  prefsHandle = Some(UniqueFileHandle(HANDLE((uintptr_t)(aPrefsHandle))));
  if (!aPrefsHandle) {
    return false;
  }

  FileDescriptor::UniquePlatformHandle handle(
      HANDLE((uintptr_t)(aPrefMapHandle)));
  if (!aPrefMapHandle) {
    return false;
  }

  mPrefMapHandle.emplace(std::move(handle));
#endif

  mPrefsLen = Some((uintptr_t)(aPrefsLen));
  if (!aPrefsLen) {
    return false;
  }

  mPrefMapSize = Some((uintptr_t)(aPrefMapSize));
  if (!aPrefMapSize) {
    return false;
  }

#if defined(ANDROID) || defined(XP_IOS)
  // Android/iOS is different; get the FD via gPrefsFd instead of a fixed fd.
  MOZ_RELEASE_ASSERT(gPrefsFd != -1);
  prefsHandle = Some(UniqueFileHandle(gPrefsFd));

  mPrefMapHandle.emplace(UniqueFileHandle(gPrefMapFd));
#elif XP_UNIX
  prefsHandle = Some(UniqueFileHandle(kPrefsFileDescriptor));

  mPrefMapHandle.emplace(UniqueFileHandle(kPrefMapFileDescriptor));
#endif

  if (prefsHandle.isNothing() || mPrefsLen.isNothing() ||
      mPrefMapHandle.isNothing() || mPrefMapSize.isNothing()) {
    return false;
  }

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(mPrefMapHandle.ref(), *mPrefMapSize);

  // Set up early prefs from the shared memory.
  if (!mShmem.SetHandle(std::move(*prefsHandle), /* read_only */ true)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }
  if (!mShmem.Map(*mPrefsLen)) {
    NS_ERROR("failed to map shared memory in the child");
    return false;
  }
  Preferences::DeserializePreferences(static_cast<char*>(mShmem.memory()),
                                      *mPrefsLen);

  return true;
}

const FileDescriptor& SharedPreferenceDeserializer::GetPrefMapHandle() const {
  MOZ_ASSERT(mPrefMapHandle.isSome());

  return mPrefMapHandle.ref();
}

#ifdef XP_UNIX
// On Unix, file descriptors are per-process. This value is used when mapping
// a parent process handle to a content process handle.
static const int kJSInitFileDescriptor = 11;
#endif

void ExportSharedJSInit(mozilla::ipc::GeckoChildProcessHost& procHost,
                        std::vector<std::string>& aExtraOpts) {
#if defined(ANDROID) || defined(XP_IOS)
  // The code to support Android/iOS is added in a follow-up patch.
  return;
#else
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  const mozilla::UniqueFileHandle& uniqHandle = shmem.Handle();
  size_t len = shmem.Content().Length();

  // If the file is not found or the content is empty, then we would start the
  // content process without this optimization.
  if (!uniqHandle || !len) {
    return;
  }

  mozilla::detail::FileHandleType handle = uniqHandle.get();
  // command line: [-jsInitHandle handle] -jsInitLen length
#  if defined(XP_WIN)
  // Record the handle as to-be-shared, and pass it via a command flag.
  procHost.AddHandleToShare(HANDLE(handle));
  geckoargs::sJsInitHandle.Put((uintptr_t)(HANDLE(handle)), aExtraOpts);
#  else
  // In contrast, Unix fds are per-process. So remap the fd to a fixed one that
  // will be used in the child.
  // XXX: bug 1440207 is about improving how fixed fds are used.
  //
  // Note: on Android, AddFdToRemap() sets up the fd to be passed via a Parcel,
  // and the fixed fd isn't used. However, we still need to mark it for
  // remapping so it doesn't get closed in the child.
  procHost.AddFdToRemap(handle, kJSInitFileDescriptor);
#  endif

  // Pass the lengths via command line flags.
  geckoargs::sJsInitLen.Put((uintptr_t)(len), aExtraOpts);
#endif
}

bool ImportSharedJSInit(uint64_t aJsInitHandle, uint64_t aJsInitLen) {
  // This is an optimization, and as such we can safely recover if the command
  // line argument are not provided.
  if (!aJsInitLen) {
    return true;
  }

#ifdef XP_WIN
  if (!aJsInitHandle) {
    return true;
  }
#endif

#ifdef XP_WIN
  base::SharedMemoryHandle handle(HANDLE((uintptr_t)(aJsInitHandle)));
  if (!aJsInitHandle) {
    return false;
  }
#endif

  size_t len = (uintptr_t)(aJsInitLen);
  if (!aJsInitLen) {
    return false;
  }

#ifdef XP_UNIX
  auto handle = UniqueFileHandle(kJSInitFileDescriptor);
#endif

  // Initialize the shared memory with the file handle and size of the content
  // of the self-hosted Xdr.
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  if (!shmem.InitFromChild(std::move(handle), len)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }

  return true;
}

}  // namespace ipc
}  // namespace mozilla
