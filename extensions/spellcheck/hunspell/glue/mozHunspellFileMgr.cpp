/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozHunspellFileMgr.h"

#include "mozilla/DebugOnly.h"
#include "nsContentUtils.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"

using namespace mozilla;

FileMgr::FileMgr(const char* aFilename, const char* aKey)
{
  DebugOnly<Result<Ok, nsresult>> result = Open(nsDependentCString(aFilename));
  NS_WARNING_ASSERTION(result.value.isOk(), "Failed to open Hunspell file");
}

Result<Ok, nsresult>
FileMgr::Open(const nsACString& aPath)
{
  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(NS_NewURI(getter_AddRefs(uri), aPath));

  nsCOMPtr<nsIChannel> channel;
  MOZ_TRY(NS_NewChannel(
    getter_AddRefs(channel),
    uri,
    nsContentUtils::GetSystemPrincipal(),
    nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
    nsIContentPolicy::TYPE_OTHER));

  MOZ_TRY(channel->Open2(getter_AddRefs(mStream)));
  return Ok();
}

Result<Ok, nsresult>
FileMgr::ReadLine(nsACString& aLine)
{
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

bool
FileMgr::getline(std::string& aResult)
{
  nsAutoCString line;
  auto res = ReadLine(line);
  if (res.isErr()) {
    return false;
  }

  aResult.assign(line.BeginReading(), line.Length());
  return true;
}
