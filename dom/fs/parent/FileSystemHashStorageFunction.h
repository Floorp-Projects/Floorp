/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMHASHSTORAGEFUNCTION_H_
#define DOM_FS_PARENT_FILESYSTEMHASHSTORAGEFUNCTION_H_

#include "mozIStorageFunction.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::fs::data {

class FileSystemHashStorageFunction final : public mozIStorageFunction {
 private:
  ~FileSystemHashStorageFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

}  // namespace mozilla::dom::fs::data

#endif  // DOM_FS_PARENT_FILESYSTEMHASHSTORAGEFUNCTION_H_
