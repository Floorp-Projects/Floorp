/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>

#include "mozHunspellRLBoxHost.h"
#include "mozilla/DebugOnly.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"

#include "hunspell_csutil.hxx"

using namespace mozilla;

mozHunspellFileMgrHost::mozHunspellFileMgrHost(const nsCString& aFilename) {
  DebugOnly<Result<Ok, nsresult>> result = Open(aFilename);
  NS_WARNING_ASSERTION(result.value.isOk(), "Failed to open Hunspell file");
}

Result<Ok, nsresult> mozHunspellFileMgrHost::Open(const nsCString& aPath) {
  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(NS_NewURI(getter_AddRefs(uri), aPath));

  nsCOMPtr<nsIChannel> channel;
  MOZ_TRY(NS_NewChannel(
      getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT,
      nsIContentPolicy::TYPE_OTHER));

  MOZ_TRY(channel->Open(getter_AddRefs(mStream)));
  return Ok();
}

Result<Ok, nsresult> mozHunspellFileMgrHost::ReadLine(nsCString& aLine) {
  if (!mStream) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  bool ok;
  MOZ_TRY(NS_ReadLine(mStream.get(), &mLineBuffer, aLine, &ok));
  if (!ok) {
    mStream = nullptr;
  }

  mLineNum++;
  return Ok();
}

bool mozHunspellFileMgrHost::GetLine(std::string& aResult) {
  nsAutoCString line;
  auto res = ReadLine(line);
  if (res.isErr()) {
    return false;
  }

  aResult.assign(line.BeginReading(), line.Length());
  return true;
}

/* static */
uint32_t mozHunspellCallbacks::sCurrentFreshId = 0;
/* static */
mozilla::detail::StaticRWLock mozHunspellCallbacks::sFileMgrMapLock;
/* static */
std::map<uint32_t, std::unique_ptr<mozHunspellFileMgrHost>>
    mozHunspellCallbacks::sFileMgrMap;
/* static */
std::set<nsCString> mozHunspellCallbacks::sFileMgrAllowList;

/* static */
void mozHunspellCallbacks::AllowFile(const nsCString& aFilename) {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);
  sFileMgrAllowList.insert(aFilename);
}

/* static */
void mozHunspellCallbacks::Clear() {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);
  sCurrentFreshId = 0;
  sFileMgrMap.clear();
  sFileMgrAllowList.clear();
}

/* static */
tainted_hunspell<uint32_t> mozHunspellCallbacks::CreateFilemgr(
    rlbox_sandbox_hunspell& aSandbox,
    tainted_hunspell<const char*> t_aFilename) {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);

  return t_aFilename.copy_and_verify_string(
      [&](std::unique_ptr<char[]> aFilename) {
        nsCString cFilename = nsDependentCString(aFilename.get());

        // Ensure that the filename is in the allowlist
        auto it = sFileMgrAllowList.find(cFilename);
        MOZ_RELEASE_ASSERT(it != sFileMgrAllowList.end());

        // Get new id
        uint32_t freshId = GetFreshId();
        // Save mapping of id to file manager
        sFileMgrMap[freshId] = std::unique_ptr<mozHunspellFileMgrHost>(
            new mozHunspellFileMgrHost(cFilename));

        return freshId;
      });
}

/* static */
uint32_t mozHunspellCallbacks::GetFreshId() {
  // i is uint64_t to prevent overflow during loop increment which would cause
  // an infinite loop
  for (uint64_t i = sCurrentFreshId; i < std::numeric_limits<uint32_t>::max();
       i++) {
    auto it = sFileMgrMap.find(i);
    if (it == sFileMgrMap.end()) {
      // set sCurrentFreshId to the next (possibly) available id
      sCurrentFreshId = i + 1;
      return static_cast<uint32_t>(i);
    }
  }

  MOZ_CRASH("Ran out of unique file ids for hunspell dictionaries");
}

/* static */
mozHunspellFileMgrHost& mozHunspellCallbacks::GetMozHunspellFileMgrHost(
    tainted_hunspell<uint32_t> t_aFd) {
  mozilla::detail::StaticAutoReadLock lock(sFileMgrMapLock);
  uint32_t aFd = t_aFd.copy_and_verify([](uint32_t aFd) { return aFd; });
  auto iter = sFileMgrMap.find(aFd);
  MOZ_RELEASE_ASSERT(iter != sFileMgrMap.end());
  return *(iter->second.get());
}

/* static */
tainted_hunspell<bool> mozHunspellCallbacks::GetLine(
    rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aFd,
    tainted_hunspell<char**> t_aLinePtr) {
  mozHunspellFileMgrHost& inst =
      mozHunspellCallbacks::GetMozHunspellFileMgrHost(t_aFd);
  std::string line;
  bool ok = inst.GetLine(line);
  if (ok) {
    // copy the line into the sandbox
    size_t size = line.size() + 1;
    tainted_hunspell<char*> t_line = aSandbox.malloc_in_sandbox<char>(size);
    MOZ_RELEASE_ASSERT(t_line);
    rlbox::memcpy(aSandbox, t_line, line.c_str(), size);
    *t_aLinePtr = t_line;
  } else {
    *t_aLinePtr = nullptr;
  }
  return ok;
}

/* static */
tainted_hunspell<int> mozHunspellCallbacks::GetLineNum(
    rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aFd) {
  mozHunspellFileMgrHost& inst =
      mozHunspellCallbacks::GetMozHunspellFileMgrHost(t_aFd);
  int num = inst.GetLineNum();
  return num;
}

/* static */
void mozHunspellCallbacks::DestructFilemgr(rlbox_sandbox_hunspell& aSandbox,
                                           tainted_hunspell<uint32_t> t_aFd) {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);
  uint32_t aFd = t_aFd.copy_and_verify([](uint32_t aFd) { return aFd; });

  auto iter = sFileMgrMap.find(aFd);
  if (iter != sFileMgrMap.end()) {
    sFileMgrMap.erase(iter);
  }
}

// Callbacks for using Firefox's encoding instead of hunspell's

/* static */
tainted_hunspell<uint32_t> mozHunspellCallbacks::ToUpperCase(
    rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aChar) {
  uint32_t aChar =
      t_aChar.copy_and_verify([](uint32_t aChar) { return aChar; });
  return ::ToUpperCase(aChar);
}

/* static */
tainted_hunspell<uint32_t> mozHunspellCallbacks::ToLowerCase(
    rlbox_sandbox_hunspell& aSandbox, tainted_hunspell<uint32_t> t_aChar) {
  uint32_t aChar =
      t_aChar.copy_and_verify([](uint32_t aChar) { return aChar; });
  return ::ToLowerCase(aChar);
}

/* static */ tainted_hunspell<struct cs_info*>
mozHunspellCallbacks::GetCurrentCS(rlbox_sandbox_hunspell& aSandbox,
                                   tainted_hunspell<const char*> t_es) {
  tainted_hunspell<struct cs_info*> t_ccs =
      aSandbox.malloc_in_sandbox<struct cs_info>(256);
  MOZ_RELEASE_ASSERT(t_ccs);
  return t_es.copy_and_verify_string([&](std::unique_ptr<char[]> es) {
    struct cs_info* ccs = hunspell_get_current_cs(es.get());
    rlbox::memcpy(aSandbox, t_ccs, ccs, sizeof(struct cs_info) * 256);
    delete[] ccs;
    return t_ccs;
  });
}
