/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableFile_h
#define mozilla_dom_MutableFile_h

#include "mozilla/dom/FileHandleCommon.h"
#include "nscore.h"

template <class> struct already_AddRefed;
class nsISupports;
class nsString;
struct PRThread;

namespace mozilla {
namespace dom {

class BackgroundMutableFileChildBase;
class BlobImpl;
class File;
class FileHandleBase;

/**
 * This class provides a base for MutableFile implementations.
 * (for example IDBMutableFile provides IndexedDB specific implementation).
 */
class MutableFileBase
  : public RefCountedThreadObject
{
protected:
  BackgroundMutableFileChildBase* mBackgroundActor;

public:
  BackgroundMutableFileChildBase*
  GetBackgroundActor() const
  {
    AssertIsOnOwningThread();

    return mBackgroundActor;
  }

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  virtual const nsString&
  Name() const = 0;

  virtual const nsString&
  Type() const = 0;

  virtual bool
  IsInvalidated() = 0;

  virtual already_AddRefed<File>
  CreateFileFor(BlobImpl* aBlobImpl,
                FileHandleBase* aFileHandle) = 0;

protected:
  MutableFileBase(DEBUGONLY(PRThread* aOwningThread,)
                  BackgroundMutableFileChildBase* aActor);

  virtual ~MutableFileBase();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MutableFile_h
