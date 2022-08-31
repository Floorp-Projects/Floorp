/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemChildFactory.h"

#include "mozilla/RefPtr.h"
#include "mozilla/dom/FileSystemManagerChild.h"

namespace mozilla::dom::fs {

already_AddRefed<FileSystemManagerChild> FileSystemChildFactory::Create()
    const {
  return MakeAndAddRef<FileSystemManagerChild>();
}

}  // namespace mozilla::dom::fs
