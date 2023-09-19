/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_
#define DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_

#include "mozilla/dom/IterableIterator.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class FileSystemManager;
class IterableIteratorBase;
class Promise;

// XXX This class isn't used to support iteration anymore. `Impl` should be
// extracted elsewhere and `FileSystemDirectoryIterator` should be removed
// completely
class FileSystemDirectoryIterator : public nsISupports, public nsWrapperCache {
 public:
  class Impl {
   public:
    NS_INLINE_DECL_REFCOUNTING(Impl)

    virtual already_AddRefed<Promise> Next(nsIGlobalObject* aGlobal,
                                           RefPtr<FileSystemManager>& aManager,
                                           ErrorResult& aError) = 0;

   protected:
    virtual ~Impl() = default;
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemDirectoryIterator)

  explicit FileSystemDirectoryIterator(nsIGlobalObject* aGlobal,
                                       RefPtr<FileSystemManager>& aManager,
                                       RefPtr<Impl>& aImpl);

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> Next(ErrorResult& aError);

 protected:
  virtual ~FileSystemDirectoryIterator() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

 private:
  RefPtr<Impl> mImpl;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_
