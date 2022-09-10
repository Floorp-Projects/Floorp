/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMHASHSOURCE_H_
#define DOM_FS_PARENT_FILESYSTEMHASHSOURCE_H_

#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs::data {

struct FileSystemHashSource {
  static Result<EntryId, QMResult> GenerateHash(const EntryId& aParent,
                                                const Name& aName);

  static Result<Name, QMResult> EncodeHash(const EntryId& aEntryId);
};

}  // namespace mozilla::dom::fs::data

#endif  // DOM_FS_PARENT_FILESYSTEMHASHSOURCE_H_
