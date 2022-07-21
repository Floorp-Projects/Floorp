/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_
#define DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_

#include "ErrorList.h"
#include "mozilla/Result.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsStringFwd.h"
#include "nsString.h"

template <class T>
class nsCOMPtr;

class nsIFile;

namespace mozilla::dom::fs {

Result<nsCOMPtr<nsIFile>, QMResult> getOrCreateFile(
    const nsAString& aDatabaseFilePath, bool& aExists,
    nsCOMPtr<nsIFile>& aFile);

Result<nsString, QMResult> getFileSystemDirectory(const Origin& aOrigin);

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_
