/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemUtils.h"

namespace mozilla {
namespace dom {

namespace {

bool
TokenizerIgnoreNothing(char16_t /* aChar */)
{
  return false;
}

} // anonymous namespace

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

/* static */ bool
FileSystemUtils::IsValidRelativeDOMPath(const nsAString& aPath,
                                        nsTArray<nsString>& aParts)
{
  // We don't allow empty relative path to access the root.
  if (aPath.IsEmpty()) {
    return false;
  }

  // Leading and trailing "/" are not allowed.
  if (aPath.First() == FILESYSTEM_DOM_PATH_SEPARATOR_CHAR ||
      aPath.Last() == FILESYSTEM_DOM_PATH_SEPARATOR_CHAR) {
    return false;
  }

  NS_NAMED_LITERAL_STRING(kCurrentDir, ".");
  NS_NAMED_LITERAL_STRING(kParentDir, "..");

  // Split path and check each path component.
  nsCharSeparatedTokenizerTemplate<TokenizerIgnoreNothing>
    tokenizer(aPath, FILESYSTEM_DOM_PATH_SEPARATOR_CHAR);

  while (tokenizer.hasMoreTokens()) {
    nsDependentSubstring pathComponent = tokenizer.nextToken();
    // The path containing empty components, such as "foo//bar", is invalid.
    // We don't allow paths, such as "../foo", "foo/./bar" and "foo/../bar",
    // to walk up the directory.
    if (pathComponent.IsEmpty() ||
        pathComponent.Equals(kCurrentDir) ||
        pathComponent.Equals(kParentDir)) {
      return false;
    }

    aParts.AppendElement(pathComponent);
  }

  return true;
}

} // namespace dom
} // namespace mozilla
