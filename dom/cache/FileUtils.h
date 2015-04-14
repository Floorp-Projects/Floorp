/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_FileUtils_h
#define mozilla_dom_cache_FileUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/Types.h"
#include "nsStreamUtils.h"
#include "nsTArrayForwardDeclare.h"

struct nsID;
class nsIFile;

namespace mozilla {
namespace dom {
namespace cache {

// TODO: remove static class and use functions in cache namespace (bug 1110485)
class FileUtils final
{
public:
  enum BodyFileType
  {
    BODY_FILE_FINAL,
    BODY_FILE_TMP
  };

  static nsresult BodyCreateDir(nsIFile* aBaseDir);
  // Note that this function can only be used during the initialization of the
  // database.  We're unlikely to be able to delete the DB successfully past
  // that point due to the file being in use.
  static nsresult BodyDeleteDir(nsIFile* aBaseDir);
  static nsresult BodyGetCacheDir(nsIFile* aBaseDir, const nsID& aId,
                                  nsIFile** aCacheDirOut);

  static nsresult
  BodyStartWriteStream(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir,
                       nsIInputStream* aSource, void* aClosure,
                       nsAsyncCopyCallbackFun aCallback, nsID* aIdOut,
                       nsISupports** aCopyContextOut);

  static void
  BodyCancelWrite(nsIFile* aBaseDir, nsISupports* aCopyContext);

  static nsresult
  BodyFinalizeWrite(nsIFile* aBaseDir, const nsID& aId);

  static nsresult
  BodyOpen(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir, const nsID& aId,
           nsIInputStream** aStreamOut);

  static nsresult
  BodyDeleteFiles(nsIFile* aBaseDir, const nsTArray<nsID>& aIdList);

private:
  static nsresult
  BodyIdToFile(nsIFile* aBaseDir, const nsID& aId, BodyFileType aType,
               nsIFile** aBodyFileOut);

  FileUtils() = delete;
  ~FileUtils() = delete;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_FileUtils_h
