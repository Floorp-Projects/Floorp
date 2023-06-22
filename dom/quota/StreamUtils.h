/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_STREAMUTILS_H_
#define DOM_QUOTA_STREAMUTILS_H_

#include "nsStringFwd.h"

class nsIBinaryInputStream;
class nsIBinaryOutputStream;
template <class T>
class nsCOMPtr;
class nsIFile;
class nsIOutputStream;
enum class nsresult : uint32_t;

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom::quota {

enum FileFlag { Truncate, Update, Append };

Result<nsCOMPtr<nsIOutputStream>, nsresult> GetOutputStream(nsIFile& aFile,
                                                            FileFlag aFileFlag);

Result<nsCOMPtr<nsIBinaryOutputStream>, nsresult> GetBinaryOutputStream(
    nsIFile& aFile, FileFlag aFileFlag);

Result<nsCOMPtr<nsIBinaryInputStream>, nsresult> GetBinaryInputStream(
    nsIFile& aDirectory, const nsAString& aFilename);

}  // namespace dom::quota
}  // namespace mozilla

#endif  // DOM_QUOTA_STREAMUTILS_H_
