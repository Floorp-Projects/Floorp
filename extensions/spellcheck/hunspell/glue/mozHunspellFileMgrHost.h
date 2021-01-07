/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozHunspellFileMgrHost_h
#define mozHunspellFileMgrHost_h

#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>

#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/RWLock.h"
#include "nsIInputStream.h"
#include "nsReadLine.h"

namespace mozilla {

class mozHunspellFileMgrHost final {
 public:
  /**
   * aFilename must be a local file/jar URI for the file to load.
   *
   * aKey is the decription key for encrypted Hunzip files, and is
   * unsupported. The argument is there solely for compatibility.
   */
  explicit mozHunspellFileMgrHost(const char* aFilename,
                                  const char* aKey = nullptr);
  ~mozHunspellFileMgrHost() = default;

  bool GetLine(std::string& aResult);
  int GetLineNum() const { return mLineNum; }

 private:
  mozilla::Result<mozilla::Ok, nsresult> Open(const nsACString& aPath);

  mozilla::Result<mozilla::Ok, nsresult> ReadLine(nsACString& aLine);

  int mLineNum = 0;
  nsCOMPtr<nsIInputStream> mStream;
  nsLineBuffer<char> mLineBuffer;
};

class mozHunspellCallbacks {
 public:
  // APIs invoked by the sandboxed hunspell file manager
  static uint32_t CreateFilemgr(const char* aFilename, const char* aKey);
  static bool GetLine(uint32_t aFd, char** aLinePtr);
  static int GetLineNum(uint32_t aFd);
  static void DestructFilemgr(uint32_t aFd);

 private:
  /**
   * sFileMgrMap holds a map between unique uint32_t
   * integers and mozHunspellFileMgrHost instances
   */
  static std::map<uint32_t, std::unique_ptr<mozHunspellFileMgrHost>>
      sFileMgrMap;
  /**
   * Reader-writer lock for the sFileMgrMap
   */
  static mozilla::detail::StaticRWLock sFileMgrMapLock;
  /**
   * Tracks the next possibly unused id for sFileMgrMap
   */
  static uint32_t sCurrentFreshId;
  /**
   * Returns an unused id for sFileMgrMap
   */
  static uint32_t GetFreshId();
  /**
   * Returns the mozHunspellFileMgrHost for the given uint32_t id
   */
  static mozHunspellFileMgrHost& GetMozHunspellFileMgrHost(uint32_t aFd);
};
}  // namespace mozilla

#endif
