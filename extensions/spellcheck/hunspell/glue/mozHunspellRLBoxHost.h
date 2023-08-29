/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozHunspellRLBoxHost_h
#define mozHunspellRLBoxHost_h

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>

#include "RLBoxHunspell.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/RWLock.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsReadLine.h"

namespace mozilla {

class mozHunspellFileMgrHost final {
 public:
  /**
   * aFilename must be a local file/jar URI for the file to load.
   */
  explicit mozHunspellFileMgrHost(const nsCString& aFilename);
  ~mozHunspellFileMgrHost() = default;

  bool GetLine(nsACString& aResult);
  int GetLineNum() const { return mLineNum; }

  static Result<int64_t, nsresult> GetSize(const nsCString& aFilename);

 private:
  static mozilla::Result<mozilla::Ok, nsresult> Open(
      const nsCString& aPath, nsCOMPtr<nsIChannel>& aChannel,
      nsCOMPtr<nsIInputStream>& aStream);

  mozilla::Result<mozilla::Ok, nsresult> ReadLine(nsACString& aLine);

  int mLineNum = 0;
  nsCOMPtr<nsIInputStream> mStream;
  nsLineBuffer<char> mLineBuffer;
};

class mozHunspellCallbacks {
 public:
  // APIs invoked by the sandboxed hunspell file manager
  static tainted_hunspell<uint32_t> CreateFilemgr(
      rlbox_sandbox_hunspell& aSandbox,
      tainted_hunspell<const char*> t_aFilename);
  static tainted_hunspell<bool> GetLine(rlbox_sandbox_hunspell& aSandbox,
                                        tainted_hunspell<uint32_t> t_aFd,
                                        tainted_hunspell<char**> t_aLinePtr);
  static tainted_hunspell<int> GetLineNum(rlbox_sandbox_hunspell& aSandbox,
                                          tainted_hunspell<uint32_t> t_aFd);
  static void DestructFilemgr(rlbox_sandbox_hunspell& aSandbox,
                              tainted_hunspell<uint32_t> t_aFd);

  // APIs necessary for hunspell UTF encoding
  static tainted_hunspell<uint32_t> ToUpperCase(
      rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aChar);
  static tainted_hunspell<uint32_t> ToLowerCase(
      rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aChar);
  static tainted_hunspell<struct cs_info*> GetCurrentCS(
      rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<const char*> t_es);

 protected:
  // API called by RLBox

  /**
   * Add filename to allow list.
   */
  static void AllowFile(const nsCString& aFilename);
  friend RLBoxHunspell* RLBoxHunspell::Create(const nsCString& affpath,
                                              const nsCString& dpath);
  /**
   * Clear allow list and map of hunspell file managers.
   */
  static void Clear();
  friend RLBoxHunspell::~RLBoxHunspell();

 private:
  /**
   * sFileMgrMap holds a map between unique uint32_t
   * integers and mozHunspellFileMgrHost instances
   */
  static std::map<uint32_t, std::unique_ptr<mozHunspellFileMgrHost>>
      sFileMgrMap;

  /**
   * sFileMgrAllowList contains the filenames of the dictionary files hunspell
   * is allowed to open
   */
  static std::set<nsCString> sFileMgrAllowList;
  /**
   * Reader-writer lock for the sFileMgrMap
   */
  static mozilla::StaticRWLock sFileMgrMapLock;
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
  static mozHunspellFileMgrHost& GetMozHunspellFileMgrHost(
      tainted_hunspell<uint32_t> t_aFd);
};
}  // namespace mozilla

#endif
