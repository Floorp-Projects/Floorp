/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryIterator.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemDirectoryIteratorBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemDirectoryIterator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemDirectoryIterator);
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemDirectoryIterator);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemDirectoryIterator, mGlobal);

FileSystemDirectoryIterator::FileSystemDirectoryIterator(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<Impl>& aImpl)
    : mGlobal(aGlobal), mManager(aManager), mImpl(aImpl) {}

// WebIDL Boilerplate

nsIGlobalObject* FileSystemDirectoryIterator::GetParentObject() const {
  return mGlobal;
}

JSObject* FileSystemDirectoryIterator::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemDirectoryIterator_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

already_AddRefed<Promise> FileSystemDirectoryIterator::Next(
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  MOZ_ASSERT(mImpl);
  return mImpl->Next(mGlobal, mManager, aError);
}

}  // namespace mozilla::dom
