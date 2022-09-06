/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_ARRAYAPPENDABLE_H_
#define DOM_FS_ARRAYAPPENDABLE_H_

#include "nsTArray.h"

class nsIGlobalObject;

namespace mozilla::dom {

class FileSystemManager;

namespace fs {

class FileSystemEntryMetadata;

class ArrayAppendable {
 public:
  virtual void append(nsIGlobalObject* aGlobal,
                      RefPtr<FileSystemManager>& aManager,
                      const nsTArray<FileSystemEntryMetadata>& aBatch) = 0;

 protected:
  virtual ~ArrayAppendable() = default;
};

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_ARRAYAPPENDABLE_H_
