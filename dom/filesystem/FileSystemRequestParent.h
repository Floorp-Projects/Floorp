/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemRequestParent_h
#define mozilla_dom_FileSystemRequestParent_h

#include "mozilla/dom/PFileSystemRequestParent.h"
#include "mozilla/dom/FileSystemBase.h"

namespace mozilla {
namespace dom {

class FileSystemParams;
class FileSystemTaskParentBase;

class FileSystemRequestParent final : public PFileSystemRequestParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemRequestParent)

public:
  FileSystemRequestParent();

  const nsCString&
  PermissionName() const
  {
    return mPermissionName;
  }

  FileSystemBase::ePermissionCheckType
  PermissionCheckType() const
  {
    return mFileSystem ? mFileSystem->PermissionCheckType()
                       : FileSystemBase::eNotSet;
  }

  bool
  Initialize(const FileSystemParams& aParams);

  void
  Start();

  bool Destroyed() const
  {
    return mDestroyed;
  }

  virtual void
  ActorDestroy(ActorDestroyReason why) override;

private:
  ~FileSystemRequestParent();

  RefPtr<FileSystemBase> mFileSystem;
  RefPtr<FileSystemTaskParentBase> mTask;

  nsCString mPermissionName;

  bool mDestroyed;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemRequestParent_h
