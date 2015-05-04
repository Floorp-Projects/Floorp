/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemUtils.h"

#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

// static
void
FileSystemUtils::LocalPathToNormalizedPath(const nsAString& aLocal,
                                           nsAString& aNorm)
{
  nsString result;
  result = aLocal;
#if defined(XP_WIN)
  char16_t* cur = result.BeginWriting();
  char16_t* end = result.EndWriting();
  for (; cur < end; ++cur) {
    if (char16_t('\\') == *cur)
      *cur = char16_t('/');
  }
#endif
  aNorm = result;
}

// static
void
FileSystemUtils::NormalizedPathToLocalPath(const nsAString& aNorm,
                                           nsAString& aLocal)
{
  nsString result;
  result = aNorm;
#if defined(XP_WIN)
  char16_t* cur = result.BeginWriting();
  char16_t* end = result.EndWriting();
  for (; cur < end; ++cur) {
    if (char16_t('/') == *cur)
      *cur = char16_t('\\');
  }
#endif
  aLocal = result;
}

// static
bool
FileSystemUtils::IsDescendantPath(const nsAString& aPath,
                                  const nsAString& aDescendantPath)
{
  // The descendant path should begin with its ancestor path.
  nsAutoString prefix;
  prefix = aPath + NS_LITERAL_STRING(FILESYSTEM_DOM_PATH_SEPARATOR);

  // Check the sub-directory path to see if it has the parent path as prefix.
  if (aDescendantPath.Length() < prefix.Length() ||
      !StringBeginsWith(aDescendantPath, prefix)) {
    return false;
  }

  return true;
}

// static
bool
FileSystemUtils::IsParentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

} // namespace dom
} // namespace mozilla
