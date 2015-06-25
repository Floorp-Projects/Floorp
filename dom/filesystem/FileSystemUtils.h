/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemUtils_h
#define mozilla_dom_FileSystemUtils_h

#include "nsString.h"

namespace mozilla {
namespace dom {

#define FILESYSTEM_DOM_PATH_SEPARATOR "/"

/*
 * This class is for error handling.
 * All methods in this class are static.
 */
class FileSystemUtils
{
public:
  /*
   * Convert the path separator to "/".
   */
  static void
  LocalPathToNormalizedPath(const nsAString& aLocal, nsAString& aNorm);

  /*
   * Convert the normalized path separator "/" to the system dependent path
   * separator, which is "/" on Mac and Linux, and "\" on Windows.
   */
  static void
  NormalizedPathToLocalPath(const nsAString& aNorm, nsAString& aLocal);

  /*
   * Return true if aDescendantPath is a descendant of aPath. Both aPath and
   * aDescendantPath are absolute DOM path.
   */
  static bool
  IsDescendantPath(const nsAString& aPath, const nsAString& aDescendantPath);

  static bool
  IsParentProcess();

  static const char16_t kSeparatorChar = char16_t('/');
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemUtils_h
