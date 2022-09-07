/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHandle.h"

#include "FileSystemDirectoryHandle.h"
#include "FileSystemFileHandle.h"
#include "fs/FileSystemRequestHandler.h"
#include "js/StructuredClone.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StorageManager.h"
#include "xpcpublic.h"

namespace mozilla::dom {

namespace {

bool ConstructHandleMetadata(JSContext* aCx, JSStructuredCloneReader* aReader,
                             const bool aDirectory,
                             fs::FileSystemEntryMetadata& aMetadata) {
  using namespace mozilla::dom::fs;

  EntryId entryId;
  if (!entryId.SetLength(32u, fallible)) {
    return false;
  }

  if (!JS_ReadBytes(aReader, static_cast<void*>(entryId.BeginWriting()), 32u)) {
    return false;
  }

  JS::Rooted<JSString*> tmpVal(aCx);
  if (!JS_ReadString(aReader, &tmpVal)) {
    return false;
  }

  Name name;
  if (!AssignJSString(aCx, name, tmpVal)) {
    return false;
  }

  aMetadata = fs::FileSystemEntryMetadata(entryId, name, aDirectory);
  return true;
}

}  // namespace

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemHandle)

NS_IMPL_CYCLE_COLLECTION_CLASS(FileSystemHandle)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FileSystemHandle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  // Don't unlink mManager!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FileSystemHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(FileSystemHandle)

FileSystemHandle::FileSystemHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    const fs::FileSystemEntryMetadata& aMetadata,
    fs::FileSystemRequestHandler* aRequestHandler)
    : mGlobal(aGlobal),
      mManager(aManager),
      mMetadata(aMetadata),
      mRequestHandler(aRequestHandler) {
  MOZ_ASSERT(!mMetadata.entryId().IsEmpty());
}

// WebIDL Boilerplate

nsIGlobalObject* FileSystemHandle::GetParentObject() const { return mGlobal; }

JSObject* FileSystemHandle::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return FileSystemHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

void FileSystemHandle::GetName(nsAString& aResult) {
  aResult = mMetadata.entryName();
}

already_AddRefed<Promise> FileSystemHandle::IsSameEntry(
    FileSystemHandle& aOther, ErrorResult& aError) const {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

// [Serializable] implementation

// static
already_AddRefed<FileSystemHandle> FileSystemHandle::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  uint32_t kind = static_cast<uint32_t>(FileSystemHandleKind::EndGuard_);

  if (!JS_ReadBytes(aReader, reinterpret_cast<void*>(&kind),
                    sizeof(uint32_t))) {
    return nullptr;
  }

  if (kind == static_cast<uint32_t>(FileSystemHandleKind::Directory)) {
    RefPtr<FileSystemHandle> result =
        FileSystemHandle::ConstructDirectoryHandle(aCx, aGlobal, aReader);
    return result.forget();
  }

  if (kind == static_cast<uint32_t>(FileSystemHandleKind::File)) {
    RefPtr<FileSystemHandle> result =
        FileSystemHandle::ConstructFileHandle(aCx, aGlobal, aReader);
    return result.forget();
  }

  return nullptr;
}

bool FileSystemHandle::WriteStructuredClone(
    JSContext* aCx, JSStructuredCloneWriter* aWriter) const {
  auto kind = static_cast<uint32_t>(Kind());
  if (NS_WARN_IF(!JS_WriteBytes(aWriter, static_cast<void*>(&kind),
                                sizeof(uint32_t)))) {
    return false;
  }

  if (NS_WARN_IF(!JS_WriteBytes(
          aWriter, static_cast<const void*>(mMetadata.entryId().get()),
          mMetadata.entryId().Length()))) {
    return false;
  }

  JS::Rooted<JS::Value> nameValue(aCx);
  if (NS_WARN_IF(!xpc::StringToJsval(aCx, mMetadata.entryName(), &nameValue))) {
    return false;
  }
  JS::Rooted<JSString*> name(aCx, nameValue.toString());

  return JS_WriteString(aWriter, name);
}

// static
already_AddRefed<FileSystemFileHandle> FileSystemHandle::ConstructFileHandle(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  using namespace mozilla::dom::fs;

  FileSystemEntryMetadata metadata;
  if (!ConstructHandleMetadata(aCx, aReader, /* aDirectory */ false,
                               metadata)) {
    return nullptr;
  }

  // XXX Get the manager from Navigator!
  auto fileSystemManager = MakeRefPtr<FileSystemManager>(aGlobal, nullptr);

  RefPtr<FileSystemFileHandle> fsHandle =
      new FileSystemFileHandle(aGlobal, fileSystemManager, metadata);

  return fsHandle.forget();
}

// static
already_AddRefed<FileSystemDirectoryHandle>
FileSystemHandle::ConstructDirectoryHandle(JSContext* aCx,
                                           nsIGlobalObject* aGlobal,
                                           JSStructuredCloneReader* aReader) {
  using namespace mozilla::dom::fs;

  FileSystemEntryMetadata metadata;
  if (!ConstructHandleMetadata(aCx, aReader, /* aDirectory */ true, metadata)) {
    return nullptr;
  }

  // XXX Get the manager from Navigator!
  auto fileSystemManager = MakeRefPtr<FileSystemManager>(aGlobal, nullptr);

  RefPtr<FileSystemDirectoryHandle> fsHandle =
      new FileSystemDirectoryHandle(aGlobal, fileSystemManager, metadata);

  return fsHandle.forget();
}

}  // namespace mozilla::dom
