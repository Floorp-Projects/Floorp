/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMCHILDFACTORY_H_
#define DOM_FS_FILESYSTEMCHILDFACTORY_H_

#include "mozilla/AlreadyAddRefed.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class FileSystemManagerChild;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom::fs {

class FileSystemChildFactory {
 public:
  virtual already_AddRefed<FileSystemManagerChild> Create() const;

  virtual ~FileSystemChildFactory() = default;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_FILESYSTEMCHILDFACTORY_H_
