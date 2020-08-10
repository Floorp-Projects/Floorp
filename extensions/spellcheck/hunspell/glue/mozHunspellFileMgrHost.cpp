/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>

#include "mozHunspellFileMgrHost.h"
#include "mozilla/DebugOnly.h"
#include "nsContentUtils.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"

using namespace mozilla;

mozHunspellFileMgrHost::mozHunspellFileMgrHost(const char* aFilename,
                                               const char* aKey) {
  DebugOnly<Result<Ok, nsresult>> result = Open(nsDependentCString(aFilename));
  NS_WARNING_ASSERTION(result.value.isOk(), "Failed to open Hunspell file");
}

Result<Ok, nsresult> mozHunspellFileMgrHost::Open(const nsACString& aPath) {
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

Result<Ok, nsresult> mozHunspellFileMgrHost::ReadLine(nsACString& aLine) {
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
uint32_t mozHunspellCallbacks::CreateFilemgr(const char* aFilename,
                                             const char* aKey) {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);
  uint32_t freshId = GetFreshId();
  sFileMgrMap[freshId] = std::unique_ptr<mozHunspellFileMgrHost>(
      new mozHunspellFileMgrHost(aFilename, aKey));
  return freshId;
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
    uint32_t aFd) {
  mozilla::detail::StaticAutoReadLock lock(sFileMgrMapLock);
  auto iter = sFileMgrMap.find(aFd);
  MOZ_RELEASE_ASSERT(iter != sFileMgrMap.end());
  return *(iter->second.get());
}

/* static */
bool mozHunspellCallbacks::GetLine(uint32_t aFd, char** aLinePtr) {
  mozHunspellFileMgrHost& inst =
      mozHunspellCallbacks::GetMozHunspellFileMgrHost(aFd);
  std::string line;
  bool ok = inst.GetLine(line);
  if (ok) {
    *aLinePtr = static_cast<char*>(malloc(line.size() + 1));
    strcpy(*aLinePtr, line.c_str());
  } else {
    *aLinePtr = nullptr;
  }
  return ok;
}

/* static */
int mozHunspellCallbacks::GetLineNum(uint32_t aFd) {
  mozHunspellFileMgrHost& inst =
      mozHunspellCallbacks::GetMozHunspellFileMgrHost(aFd);
  int num = inst.GetLineNum();
  return num;
}

/* static */
void mozHunspellCallbacks::DestructFilemgr(uint32_t aFd) {
  mozilla::detail::StaticAutoWriteLock lock(sFileMgrMapLock);

  auto iter = sFileMgrMap.find(aFd);
  if (iter != sFileMgrMap.end()) {
    sFileMgrMap.erase(iter);
  }
}
