/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileDescriptorSetParent.h"

namespace mozilla {
namespace dom {

FileDescriptorSetParent::FileDescriptorSetParent(
                                          const FileDescriptor& aFileDescriptor)
{
  mFileDescriptors.AppendElement(aFileDescriptor);
}

FileDescriptorSetParent::~FileDescriptorSetParent()
{
}

void
FileDescriptorSetParent::ForgetFileDescriptors(
                                     nsTArray<FileDescriptor>& aFileDescriptors)
{
  aFileDescriptors.Clear();
  mFileDescriptors.SwapElements(aFileDescriptors);
}

void
FileDescriptorSetParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005157
}

bool
FileDescriptorSetParent::RecvAddFileDescriptor(
                                          const FileDescriptor& aFileDescriptor)
{
  mFileDescriptors.AppendElement(aFileDescriptor);
  return true;
}

} // dom namespace
} // mozilla namespace
