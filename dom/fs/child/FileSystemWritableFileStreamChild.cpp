/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStreamChild.h"

#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemWritableFileStream.h"

namespace mozilla::dom {

FileSystemWritableFileStreamChild::FileSystemWritableFileStreamChild()
    : mStream(nullptr) {
  LOG(("Created new WritableFileStreamChild %p", this));
}

FileSystemWritableFileStreamChild::~FileSystemWritableFileStreamChild() =
    default;

void FileSystemWritableFileStreamChild::SetStream(
    FileSystemWritableFileStream* aStream) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!mStream);

  mStream = aStream;
}

void FileSystemWritableFileStreamChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("Destroy WritableFileStreamChild %p", this));

  if (mStream) {
    mStream->ClearActor();
    mStream = nullptr;
  }
}

}  // namespace mozilla::dom
