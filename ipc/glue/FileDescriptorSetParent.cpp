/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorSetParent.h"

namespace mozilla::ipc {

FileDescriptorSetParent::FileDescriptorSetParent(
    const FileDescriptor& aFileDescriptor) {
  mFileDescriptors.AppendElement(aFileDescriptor);
}

FileDescriptorSetParent::~FileDescriptorSetParent() = default;

void FileDescriptorSetParent::ForgetFileDescriptors(
    nsTArray<FileDescriptor>& aFileDescriptors) {
  aFileDescriptors = std::move(mFileDescriptors);
}

void FileDescriptorSetParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Implement me! Bug 1005157
}

mozilla::ipc::IPCResult FileDescriptorSetParent::RecvAddFileDescriptor(
    const FileDescriptor& aFileDescriptor) {
  mFileDescriptors.AppendElement(aFileDescriptor);
  return IPC_OK();
}

}  // namespace mozilla::ipc
