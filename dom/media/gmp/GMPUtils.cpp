/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsLiteralString.h"
#include "nsCRTGlue.h"
#include "mozilla/Base64.h"
#include "nsISimpleEnumerator.h"
#include "prio.h"
#include "nsIConsoleService.h"
#include "mozIGeckoMediaPluginService.h"
#include "GMPService.h"

namespace mozilla {

void
SplitAt(const char* aDelims,
        const nsACString& aInput,
        nsTArray<nsCString>& aOutTokens)
{
  nsAutoCString str(aInput);
  char* end = str.BeginWriting();
  const char* start = nullptr;
  while (!!(start = NS_strtok(aDelims, &end))) {
    aOutTokens.AppendElement(nsCString(start));
  }
}

nsCString
ToHexString(const uint8_t * aBytes, uint32_t aLength)
{
  static const char hex[] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'a', 'b',
    'c', 'd', 'e', 'f'
  };
  nsCString str;
  for (uint32_t i = 0; i < aLength; i++) {
    char buf[3];
    buf[0] = hex[(aBytes[i] & 0xf0) >> 4];
    buf[1] = hex[aBytes[i] & 0x0f];
    buf[2] = 0;
    str.AppendASCII(buf);
  }
  return str;
}

nsCString
ToHexString(const nsTArray<uint8_t>& aBytes)
{
  return ToHexString(aBytes.Elements(), aBytes.Length());
}

bool
FileExists(nsIFile* aFile)
{
  bool exists = false;
  return aFile && NS_SUCCEEDED(aFile->Exists(&exists)) && exists;
}

DirectoryEnumerator::DirectoryEnumerator(nsIFile* aPath, Mode aMode)
  : mMode(aMode)
{
  aPath->GetDirectoryEntries(getter_AddRefs(mIter));
}

already_AddRefed<nsIFile>
DirectoryEnumerator::Next()
{
  if (!mIter) {
    return nullptr;
  }
  bool hasMore = false;
  while (NS_SUCCEEDED(mIter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mIter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsIFile> path(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) {
      continue;
    }

    if (mMode == DirsOnly) {
      bool isDirectory = false;
      rv = path->IsDirectory(&isDirectory);
      if (NS_FAILED(rv) || !isDirectory) {
        continue;
      }
    }
    return path.forget();
  }
  return nullptr;
}

bool
ReadIntoArray(nsIFile* aFile,
              nsTArray<uint8_t>& aOutDst,
              size_t aMaxLength)
{
  if (!FileExists(aFile)) {
    return false;
  }

  PRFileDesc* fd = nullptr;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    return false;
  }

  int32_t length = PR_Seek(fd, 0, PR_SEEK_END);
  PR_Seek(fd, 0, PR_SEEK_SET);

  if (length < 0 || (size_t)length > aMaxLength) {
    NS_WARNING("EME file is longer than maximum allowed length");
    PR_Close(fd);
    return false;
  }
  aOutDst.SetLength(length);
  int32_t bytesRead = PR_Read(fd, aOutDst.Elements(), length);
  PR_Close(fd);
  return (bytesRead == length);
}

bool
ReadIntoString(nsIFile* aFile,
               nsCString& aOutDst,
               size_t aMaxLength)
{
  nsTArray<uint8_t> buf;
  bool rv = ReadIntoArray(aFile, buf, aMaxLength);
  if (rv) {
    buf.AppendElement(0); // Append null terminator, required by nsC*String.
    aOutDst = nsDependentCString((const char*)buf.Elements(), buf.Length() - 1);
  }
  return rv;
}

bool
GMPInfoFileParser::Init(nsIFile* aInfoFile)
{
  nsTArray<nsCString> lines;
  static const size_t MAX_GMP_INFO_FILE_LENGTH = 5 * 1024;

  nsAutoCString info;
  if (!ReadIntoString(aInfoFile, info, MAX_GMP_INFO_FILE_LENGTH)) {
    NS_WARNING("Failed to read info file in GMP process.");
    return false;
  }

  // Note: we pass "\r\n" to SplitAt so that we'll split lines delimited
  // by \n (Unix), \r\n (Windows) and \r (old MacOSX).
  SplitAt("\r\n", info, lines);

  for (nsCString line : lines) {
    // Field name is the string up to but not including the first ':'
    // character on the line.
    int32_t colon = line.FindChar(':');
    if (colon <= 0) {
      // Not allowed to be the first character.
      // Info field name must be at least one character.
      continue;
    }
    nsAutoCString key(Substring(line, 0, colon));
    ToLowerCase(key);
    key.Trim(" ");

    nsCString* value = new nsCString(Substring(line, colon + 1));
    value->Trim(" ");
    mValues.Put(key, value); // Hashtable assumes ownership of value.
  }

  return true;
}

bool
GMPInfoFileParser::Contains(const nsCString& aKey) const {
  nsCString key(aKey);
  ToLowerCase(key);
  return mValues.Contains(key);
}

nsCString
GMPInfoFileParser::Get(const nsCString& aKey) const {
  MOZ_ASSERT(Contains(aKey));
  nsCString key(aKey);
  ToLowerCase(key);
  nsCString* p = nullptr;
  if (mValues.Get(key, &p)) {
    return nsCString(*p);
  }
  return EmptyCString();
}

bool
HaveGMPFor(const nsCString& aAPI,
           nsTArray<nsCString>&& aTags)
{
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return false;
  }

  bool hasPlugin = false;
  if (NS_FAILED(mps->HasPluginForAPI(aAPI, &aTags, &hasPlugin))) {
    return false;
  }
  return hasPlugin;
}

void
LogToConsole(const nsAString& aMsg)
{
  nsCOMPtr<nsIConsoleService> console(
    do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }
  nsAutoString msg(aMsg);
  console->LogStringMessage(msg.get());
}

RefPtr<AbstractThread>
GetGMPAbstractThread()
{
  RefPtr<gmp::GeckoMediaPluginService> service =
    gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
  return service ? service->GetAbstractGMPThread() : nullptr;
}

static size_t
Align16(size_t aNumber)
{
  const size_t mask = 15; // Alignment - 1.
  return (aNumber + mask) & ~mask;
}

size_t
I420FrameBufferSizePadded(int32_t aWidth, int32_t aHeight)
{
  if (aWidth <= 0 || aHeight <= 0 || aWidth > MAX_VIDEO_WIDTH ||
      aHeight > MAX_VIDEO_HEIGHT) {
    return 0;
  }

  size_t ySize = Align16(aWidth) * Align16(aHeight);
  return ySize + (ySize / 4) * 2;
}

} // namespace mozilla
