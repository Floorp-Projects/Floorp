/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorSetChild_h__
#define mozilla_ipc_FileDescriptorSetChild_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/PFileDescriptorSetChild.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {

class ContentChild;

} // namespace dom

namespace ipc {

class BackgroundChildImpl;
class FileDescriptor;

class FileDescriptorSetChild MOZ_FINAL
  : public PFileDescriptorSetChild
{
  friend class BackgroundChildImpl;
  friend class mozilla::dom::ContentChild;

  nsTArray<FileDescriptor> mFileDescriptors;

public:
  void
  ForgetFileDescriptors(nsTArray<FileDescriptor>& aFileDescriptors);

private:
  explicit FileDescriptorSetChild(const FileDescriptor& aFileDescriptor);
  ~FileDescriptorSetChild();

  virtual bool
  RecvAddFileDescriptor(const FileDescriptor& aFileDescriptor) MOZ_OVERRIDE;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptorSetChild_h__
