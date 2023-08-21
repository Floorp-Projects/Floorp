/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMCONTENTTYPEGUESS_H_
#define DOM_FS_PARENT_FILESYSTEMCONTENTTYPEGUESS_H_

#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "nsStringFwd.h"

namespace mozilla::dom::fs {

struct FileSystemContentTypeGuess {
  static Result<ContentType, QMResult> FromPath(const Name& aPath);
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_FILESYSTEMCONTENTTYPEGUESS_H_
