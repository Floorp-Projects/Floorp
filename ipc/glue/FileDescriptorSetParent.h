/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorSetParent_h__
#define mozilla_ipc_FileDescriptorSetParent_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {

class nsIContentParent;

} // namespace dom

namespace ipc {

class BackgroundParentImpl;
class FileDescriptor;

class FileDescriptorSetParent final
  : public PFileDescriptorSetParent
{
  friend class BackgroundParentImpl;
  friend class mozilla::dom::nsIContentParent;

  nsTArray<FileDescriptor> mFileDescriptors;

public:
  void
  ForgetFileDescriptors(nsTArray<FileDescriptor>& aFileDescriptors);

private:
  explicit FileDescriptorSetParent(const FileDescriptor& aFileDescriptor);
  ~FileDescriptorSetParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvAddFileDescriptor(const FileDescriptor& aFileDescriptor) override;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptorSetParent_h__
