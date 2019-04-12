/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace ipc {

SharedPreferenceSerializer::SharedPreferenceSerializer() : mPrefMapSize(0) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::~SharedPreferenceSerializer() {
  MOZ_COUNT_DTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::SharedPreferenceSerializer(
    SharedPreferenceSerializer&& aOther)
    : mPrefMapSize(aOther.mPrefMapSize),
      mPrefMapHandle(std::move(aOther.mPrefMapHandle)),
      mShm(std::move(aOther.mShm)),
      mPrefs(std::move(aOther.mPrefs)) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

bool SharedPreferenceSerializer::SerializeToSharedMemory() {
  mPrefMapHandle =
      Preferences::EnsureSnapshot(&mPrefMapSize).ClonePlatformHandle();

  // Serialize the early prefs.
  Preferences::SerializePreferences(mPrefs);

  // Set up the shared memory.
  if (!mShm.Create(mPrefs.Length())) {
    NS_ERROR("failed to create shared memory in the parent");
    return false;
  }
  if (!mShm.Map(mPrefs.Length())) {
    NS_ERROR("failed to map shared memory in the parent");
    return false;
  }

  // Copy the serialized prefs into the shared memory.
  memcpy(static_cast<char*>(mShm.memory()), mPrefs.get(), mPrefs.Length());

  return true;
}

void SharedPreferenceSerializer::AddSharedPrefCmdLineArgs(
    mozilla::ipc::GeckoChildProcessHost& procHost,
    std::vector<std::string>& aExtraOpts) const {
  // Formats a pointer or pointer-sized-integer as a string suitable for passing
  // in an arguments list.
  auto formatPtrArg = [](auto arg) {
    return nsPrintfCString("%zu", uintptr_t(arg));
  };

#if defined(XP_WIN)
  // Record the handle as to-be-shared, and pass it via a command flag. This
  // works because Windows handles are system-wide.
  HANDLE prefsHandle = GetSharedMemoryHandle();
  procHost.AddHandleToShare(prefsHandle);
  procHost.AddHandleToShare(GetPrefMapHandle().get());
  aExtraOpts.push_back("-prefsHandle");
  aExtraOpts.push_back(formatPtrArg(prefsHandle).get());
  aExtraOpts.push_back("-prefMapHandle");
  aExtraOpts.push_back(formatPtrArg(GetPrefMapHandle().get()).get());
#else
  // In contrast, Unix fds are per-process. So remap the fd to a fixed one that
  // will be used in the child.
  // XXX: bug 1440207 is about improving how fixed fds are used.
  //
  // Note: on Android, AddFdToRemap() sets up the fd to be passed via a Parcel,
  // and the fixed fd isn't used. However, we still need to mark it for
  // remapping so it doesn't get closed in the child.
  procHost.AddFdToRemap(GetSharedMemoryHandle().fd, kPrefsFileDescriptor);
  procHost.AddFdToRemap(GetPrefMapHandle().get(), kPrefMapFileDescriptor);
#endif

  // Pass the lengths via command line flags.
  aExtraOpts.push_back("-prefsLen");
  aExtraOpts.push_back(formatPtrArg(GetPrefLength()).get());
  aExtraOpts.push_back("-prefMapSize");
  aExtraOpts.push_back(formatPtrArg(GetPrefMapSize()).get());
}

#ifdef ANDROID
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
    char* aPrefsHandleStr, char* aPrefMapHandleStr, char* aPrefsLenStr,
    char* aPrefMapSizeStr) {
  // Parses an arg containing a pointer-sized-integer.
  auto parseUIntPtrArg = [](char*& aArg) {
    // ContentParent uses %zu to print a word-sized unsigned integer. So
    // even though strtoull() returns a long long int, it will fit in a
    // uintptr_t.
    return uintptr_t(strtoull(aArg, &aArg, 10));
  };

#ifdef XP_WIN
  auto parseHandleArg = [&](char*& aArg) {
    return HANDLE(parseUIntPtrArg(aArg));
  };

  mPrefsHandle = Some(parseHandleArg(aPrefsHandleStr));
  if (aPrefsHandleStr[0] != '\0') {
    return false;
  }

  // The FileDescriptor constructor will clone this handle when constructed,
  // so store it in a UniquePlatformHandle to make sure the original gets
  // closed.
  FileDescriptor::UniquePlatformHandle handle(
      parseHandleArg(aPrefMapHandleStr));
  if (aPrefMapHandleStr[0] != '\0') {
    return false;
  }

  mPrefMapHandle.emplace(handle.get());
#endif

  mPrefsLen = Some(parseUIntPtrArg(aPrefsLenStr));
  if (aPrefsLenStr[0] != '\0') {
    return false;
  }

  mPrefMapSize = Some(parseUIntPtrArg(aPrefMapSizeStr));
  if (aPrefMapSizeStr[0] != '\0') {
    return false;
  }

#ifdef ANDROID
  // Android is different; get the FD via gPrefsFd instead of a fixed fd.
  MOZ_RELEASE_ASSERT(gPrefsFd != -1);
  mPrefsHandle = Some(base::FileDescriptor(gPrefsFd, /* auto_close */ true));

  FileDescriptor::UniquePlatformHandle handle(gPrefMapFd);
  mPrefMapHandle.emplace(handle.get());
#elif XP_UNIX
  mPrefsHandle = Some(base::FileDescriptor(kPrefsFileDescriptor,
                                           /* auto_close */ true));

  // The FileDescriptor constructor will clone this handle when constructed,
  // so store it in a UniquePlatformHandle to make sure the original gets
  // closed.
  FileDescriptor::UniquePlatformHandle handle(kPrefMapFileDescriptor);
  mPrefMapHandle.emplace(handle.get());
#endif

  if (mPrefsHandle.isNothing() || mPrefsLen.isNothing() ||
      mPrefMapHandle.isNothing() || mPrefMapSize.isNothing()) {
    return false;
  }

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(mPrefMapHandle.ref(), *mPrefMapSize);

  // Set up early prefs from the shared memory.
  if (!mShmem.SetHandle(*mPrefsHandle, /* read_only */ true)) {
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

const base::SharedMemoryHandle& SharedPreferenceDeserializer::GetPrefsHandle()
    const {
  MOZ_ASSERT(mPrefsHandle.isSome());

  return mPrefsHandle.ref();
}

const FileDescriptor& SharedPreferenceDeserializer::GetPrefMapHandle() const {
  MOZ_ASSERT(mPrefMapHandle.isSome());

  return mPrefMapHandle.ref();
}

}  // namespace ipc
}  // namespace mozilla
