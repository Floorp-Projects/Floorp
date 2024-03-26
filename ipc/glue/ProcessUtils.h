/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessUtils_h
#define mozilla_ipc_ProcessUtils_h

#include <functional>
#include <vector>

#include "mozilla/ipc/FileDescriptor.h"
#include "base/shared_memory.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost;

// You probably should call ContentChild::SetProcessName instead of calling
// this directly.
void SetThisProcessName(const char* aName);

class SharedPreferenceSerializer final {
 public:
  explicit SharedPreferenceSerializer();
  SharedPreferenceSerializer(SharedPreferenceSerializer&& aOther);
  ~SharedPreferenceSerializer();

  bool SerializeToSharedMemory(const GeckoProcessType aDestinationProcessType,
                               const nsACString& aDestinationRemoteType);

  size_t GetPrefMapSize() const { return mPrefMapSize; }
  size_t GetPrefsLength() const { return mPrefsLength; }

  const UniqueFileHandle& GetPrefsHandle() const { return mPrefsHandle; }

  const UniqueFileHandle& GetPrefMapHandle() const { return mPrefMapHandle; }

  void AddSharedPrefCmdLineArgs(GeckoChildProcessHost& procHost,
                                std::vector<std::string>& aExtraOpts) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedPreferenceSerializer);
  size_t mPrefMapSize;
  size_t mPrefsLength;
  UniqueFileHandle mPrefMapHandle;
  UniqueFileHandle mPrefsHandle;
};

class SharedPreferenceDeserializer final {
 public:
  SharedPreferenceDeserializer();
  ~SharedPreferenceDeserializer();

  bool DeserializeFromSharedMemory(uint64_t aPrefsHandle,
                                   uint64_t aPrefMapHandle, uint64_t aPrefsLen,
                                   uint64_t aPrefMapSize);

  const FileDescriptor& GetPrefMapHandle() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedPreferenceDeserializer);
  Maybe<FileDescriptor> mPrefMapHandle;
  Maybe<size_t> mPrefsLen;
  Maybe<size_t> mPrefMapSize;
  base::SharedMemory mShmem;
};

#if defined(ANDROID) || defined(XP_IOS)
// Android/iOS doesn't use -prefsHandle or -prefMapHandle. It gets those FDs
// another way.
void SetPrefsFd(int aFd);
void SetPrefMapFd(int aFd);
#endif

// Generate command line argument to spawn a child process. If the shared memory
// is not properly initialized, this would be a no-op.
void ExportSharedJSInit(GeckoChildProcessHost& procHost,
                        std::vector<std::string>& aExtraOpts);

// Initialize the content used by the JS engine during the initialization of a
// JS::Runtime.
bool ImportSharedJSInit(uint64_t aJsInitHandle, uint64_t aJsInitLen);

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_ProcessUtils_h
