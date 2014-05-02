/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileDescriptorSetParent_h__
#define mozilla_dom_FileDescriptorSetParent_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PFileDescriptorSetParent.h"
#include "nsTArray.h"

namespace mozilla {

namespace ipc {

class FileDescriptor;

} // namespace ipc

namespace dom {

class ContentParent;

class FileDescriptorSetParent MOZ_FINAL: public PFileDescriptorSetParent
{
  friend class ContentParent;

public:
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  void
  ForgetFileDescriptors(nsTArray<FileDescriptor>& aFileDescriptors);

private:
  FileDescriptorSetParent(const FileDescriptor& aFileDescriptor);
  ~FileDescriptorSetParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvAddFileDescriptor(const FileDescriptor& aFileDescriptor) MOZ_OVERRIDE;

  nsTArray<FileDescriptor> mFileDescriptors;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileDescriptorSetParent_h__
