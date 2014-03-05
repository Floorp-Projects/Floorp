/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemRequestParent_h
#define mozilla_dom_FileSystemRequestParent_h

#include "mozilla/dom/PFileSystemRequestParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

class FileSystemBase;

class FileSystemRequestParent
  : public PFileSystemRequestParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemRequestParent)
public:
  FileSystemRequestParent();

  virtual
  ~FileSystemRequestParent();

  bool
  IsRunning()
  {
    return state() == PFileSystemRequest::__Start;
  }

  bool
  Dispatch(ContentParent* aParent, const FileSystemParams& aParams);

  virtual void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
private:
  nsRefPtr<FileSystemBase> mFileSystem;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemRequestParent_h
