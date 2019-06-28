/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessUtils_h
#define mozilla_ipc_ProcessUtils_h

#include "FileDescriptor.h"
#include "base/shared_memory.h"

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost;

// You probably should call ContentChild::SetProcessName instead of calling
// this directly.
void SetThisProcessName(const char* aName);

class SharedPreferenceSerializer final {
 public:
  SharedPreferenceSerializer();
  SharedPreferenceSerializer(SharedPreferenceSerializer&& aOther);
  ~SharedPreferenceSerializer();

  bool SerializeToSharedMemory();

  base::SharedMemoryHandle GetSharedMemoryHandle() const {
    return mShm.handle();
  }

  const FileDescriptor::UniquePlatformHandle& GetPrefMapHandle() const {
    return mPrefMapHandle;
  }

  nsACString::size_type GetPrefLength() const { return mPrefs.Length(); }

  size_t GetPrefMapSize() const { return mPrefMapSize; }

  void AddSharedPrefCmdLineArgs(GeckoChildProcessHost& procHost,
                                std::vector<std::string>& aExtraOpts) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedPreferenceSerializer);
  size_t mPrefMapSize;
  FileDescriptor::UniquePlatformHandle mPrefMapHandle;
  base::SharedMemory mShm;
  nsAutoCStringN<1024> mPrefs;
};

class SharedPreferenceDeserializer final {
 public:
  SharedPreferenceDeserializer();
  ~SharedPreferenceDeserializer();

  bool DeserializeFromSharedMemory(char* aPrefsHandleStr,
                                   char* aPrefMapHandleStr, char* aPrefsLenStr,
                                   char* aPrefMapSizeStr);

  const base::SharedMemoryHandle& GetPrefsHandle() const;
  const FileDescriptor& GetPrefMapHandle() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedPreferenceDeserializer);
  Maybe<base::SharedMemoryHandle> mPrefsHandle;
  Maybe<FileDescriptor> mPrefMapHandle;
  Maybe<size_t> mPrefsLen;
  Maybe<size_t> mPrefMapSize;
  base::SharedMemory mShmem;
};

#ifdef ANDROID
// Android doesn't use -prefsHandle or -prefMapHandle. It gets those FDs
// another way.
void SetPrefsFd(int aFd);
void SetPrefMapFd(int aFd);
#endif

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_ProcessUtils_h
