/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemUtils.h"

namespace mozilla {
namespace dom {

/* static */ bool
FileSystemUtils::IsDescendantPath(nsIFile* aFile,
                                  nsIFile* aDescendantFile)
{
  nsAutoString path;
  nsresult rv = aFile->GetPath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoString descendantPath;
  rv = aDescendantFile->GetPath(descendantPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Check the sub-directory path to see if it has the parent path as prefix.
  if (descendantPath.Length() <= path.Length() ||
      !StringBeginsWith(descendantPath, path)) {
    return false;
  }

  return true;
}

} // namespace dom
} // namespace mozilla
