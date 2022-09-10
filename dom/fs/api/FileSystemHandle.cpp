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
#include "mozilla/dom/Promise-inl.h"
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemHandle)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FileSystemHandle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  // Don't unlink mManager!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FileSystemHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

  // Handles the case of "dir = createdir foo; removeEntry(foo); file =
  // createfile foo; issameentry(dir, file)"
  const bool result = mMetadata.entryId().Equals(aOther.mMetadata.entryId()) &&
                      Kind() == aOther.Kind();
  promise->MaybeResolve(result);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemHandle::Move(const nsAString& aName,
                                                 ErrorResult& aError) {
  LOG(("Move %s to %s", NS_ConvertUTF16toUTF8(mMetadata.entryName()).get(),
       NS_ConvertUTF16toUTF8(aName).get()));

  fs::EntryId parent;  // empty means same directory
  return Move(parent, aName, aError);
}

already_AddRefed<Promise> FileSystemHandle::Move(
    FileSystemDirectoryHandle& aParent, ErrorResult& aError) {
  LOG(("Move %s to %s/%s", NS_ConvertUTF16toUTF8(mMetadata.entryName()).get(),
       NS_ConvertUTF16toUTF8(aParent.mMetadata.entryName()).get(),
       NS_ConvertUTF16toUTF8(mMetadata.entryName()).get()));
  return Move(aParent, mMetadata.entryName(), aError);
}

already_AddRefed<Promise> FileSystemHandle::Move(
    FileSystemDirectoryHandle& aParent, const nsAString& aName,
    ErrorResult& aError) {
  LOG(("Move %s to %s/%s", NS_ConvertUTF16toUTF8(mMetadata.entryName()).get(),
       NS_ConvertUTF16toUTF8(aParent.mMetadata.entryName()).get(),
       NS_ConvertUTF16toUTF8(aName).get()));
  return Move(aParent.mMetadata.entryId(), aName, aError);
}

already_AddRefed<Promise> FileSystemHandle::Move(const fs::EntryId& aParentId,
                                                 const nsAString& aName,
                                                 ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  fs::Name name(aName);
  if (!aParentId.IsEmpty()) {
    fs::FileSystemChildMetadata newMetadata;
    newMetadata.parentId() = aParentId;
    newMetadata.childName() = aName;
    mRequestHandler->MoveEntry(mManager, this, mMetadata, newMetadata, promise);
  } else {
    mRequestHandler->RenameEntry(mManager, this, mMetadata, name, promise);
  }

  // Other handles to this will be broken, and the spec is ok with this, but we
  // need to update our EntryId and name
  promise->AddCallbacksWithCycleCollectedArgs(
      [name](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
             FileSystemHandle* aHandle) {
        // XXX Fix entryId!
        LOG(("Changing FileSystemHandle name from %s to %s",
             NS_ConvertUTF16toUTF8(aHandle->mMetadata.entryName()).get(),
             NS_ConvertUTF16toUTF8(name).get()));
        aHandle->mMetadata.entryName() = name;
      },
      [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
         FileSystemHandle* aHandle) {
        LOG(("reject of move for %s",
             NS_ConvertUTF16toUTF8(aHandle->mMetadata.entryName()).get()));
      },
      RefPtr(this));

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
