/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryHandle.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemDirectoryHandleBinding.h"
#include "mozilla/dom/FileSystemDirectoryIterator.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemDirectoryHandle,
                                               FileSystemHandle)
NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemDirectoryHandle, FileSystemHandle)

// WebIDL Boilerplate

JSObject* FileSystemDirectoryHandle::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemDirectoryHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

FileSystemHandleKind FileSystemDirectoryHandle::Kind() {
  return FileSystemHandleKind::Directory;
}

already_AddRefed<FileSystemDirectoryIterator>
FileSystemDirectoryHandle::Entries() {
  return MakeRefPtr<FileSystemDirectoryIterator>(GetParentObject()).forget();
}

already_AddRefed<FileSystemDirectoryIterator>
FileSystemDirectoryHandle::Keys() {
  return MakeRefPtr<FileSystemDirectoryIterator>(GetParentObject()).forget();
}

already_AddRefed<FileSystemDirectoryIterator>
FileSystemDirectoryHandle::Values() {
  return MakeRefPtr<FileSystemDirectoryIterator>(GetParentObject()).forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::GetFileHandle(
    const nsAString& aName, const FileSystemGetFileOptions& aOptions) {
  IgnoredErrorResult rv;

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::GetDirectoryHandle(
    const nsAString& aName, const FileSystemGetDirectoryOptions& aOptions) {
  IgnoredErrorResult rv;

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::RemoveEntry(
    const nsAString& aName, const FileSystemRemoveOptions& aOptions) {
  IgnoredErrorResult rv;

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::Resolve(
    FileSystemHandle& aPossibleDescendant) {
  IgnoredErrorResult rv;

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

}  // namespace mozilla::dom
