/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorSetChild.h"

namespace mozilla {
namespace ipc {

FileDescriptorSetChild::FileDescriptorSetChild(
                                         const FileDescriptor& aFileDescriptor)
{
  mFileDescriptors.AppendElement(aFileDescriptor);
}

FileDescriptorSetChild::~FileDescriptorSetChild()
{
  MOZ_ASSERT(mFileDescriptors.IsEmpty());
}

void
FileDescriptorSetChild::ForgetFileDescriptors(
                                    nsTArray<FileDescriptor>& aFileDescriptors)
{
  aFileDescriptors.Clear();
  mFileDescriptors.SwapElements(aFileDescriptors);
}

bool
FileDescriptorSetChild::RecvAddFileDescriptor(
                                         const FileDescriptor& aFileDescriptor)
{
  mFileDescriptors.AppendElement(aFileDescriptor);
  return true;
}

} // namespace ipc
} // namespace mozilla
