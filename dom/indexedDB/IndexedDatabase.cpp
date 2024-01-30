/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"

#include "IDBDatabase.h"

#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WorkerScope.h"
#include "MainThreadUtils.h"
#include "jsapi.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsQueryObject.h"
#include "nsString.h"

namespace mozilla::dom::indexedDB {
namespace {
struct MOZ_STACK_CLASS MutableFileData final {
  nsString type;
  nsString name;

  MOZ_COUNTED_DEFAULT_CTOR(MutableFileData)

  MOZ_COUNTED_DTOR(MutableFileData)
};

struct MOZ_STACK_CLASS BlobOrFileData final {
  uint32_t tag = 0;
  uint64_t size = 0;
  nsString type;
  nsString name;
  int64_t lastModifiedDate = INT64_MAX;

  MOZ_COUNTED_DEFAULT_CTOR(BlobOrFileData)

  MOZ_COUNTED_DTOR(BlobOrFileData)
};

struct MOZ_STACK_CLASS WasmModuleData final {
  uint32_t bytecodeIndex;
  uint32_t compiledIndex;
  uint32_t flags;

  explicit WasmModuleData(uint32_t aFlags)
      : bytecodeIndex(0), compiledIndex(0), flags(aFlags) {
    MOZ_COUNT_CTOR(WasmModuleData);
  }

  MOZ_COUNTED_DTOR(WasmModuleData)
};

bool StructuredCloneReadString(JSStructuredCloneReader* aReader,
                               nsCString& aString) {
  uint32_t length;
  if (!JS_ReadBytes(aReader, &length, sizeof(uint32_t))) {
    NS_WARNING("Failed to read length!");
    return false;
  }
  length = NativeEndian::swapFromLittleEndian(length);

  if (!aString.SetLength(length, fallible)) {
    NS_WARNING("Out of memory?");
    return false;
  }
  char* const buffer = aString.BeginWriting();

  if (!JS_ReadBytes(aReader, buffer, length)) {
    NS_WARNING("Failed to read type!");
    return false;
  }

  return true;
}

bool ReadFileHandle(JSStructuredCloneReader* aReader,
                    MutableFileData* aRetval) {
  static_assert(SCTAG_DOM_MUTABLEFILE == 0xFFFF8004, "Update me!");
  MOZ_ASSERT(aReader && aRetval);

  nsCString type;
  if (!StructuredCloneReadString(aReader, type)) {
    return false;
  }
  CopyUTF8toUTF16(type, aRetval->type);

  nsCString name;
  if (!StructuredCloneReadString(aReader, name)) {
    return false;
  }
  CopyUTF8toUTF16(name, aRetval->name);

  return true;
}

bool ReadBlobOrFile(JSStructuredCloneReader* aReader, uint32_t aTag,
                    BlobOrFileData* aRetval) {
  static_assert(SCTAG_DOM_BLOB == 0xffff8001 &&
                    SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE == 0xffff8002 &&
                    SCTAG_DOM_FILE == 0xffff8005,
                "Update me!");

  MOZ_ASSERT(aReader);
  MOZ_ASSERT(aTag == SCTAG_DOM_FILE ||
             aTag == SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE ||
             aTag == SCTAG_DOM_BLOB);
  MOZ_ASSERT(aRetval);

  aRetval->tag = aTag;

  uint64_t size;
  if (NS_WARN_IF(!JS_ReadBytes(aReader, &size, sizeof(uint64_t)))) {
    return false;
  }

  aRetval->size = NativeEndian::swapFromLittleEndian(size);

  nsCString type;
  if (NS_WARN_IF(!StructuredCloneReadString(aReader, type))) {
    return false;
  }

  CopyUTF8toUTF16(type, aRetval->type);

  // Blobs are done.
  if (aTag == SCTAG_DOM_BLOB) {
    return true;
  }

  MOZ_ASSERT(aTag == SCTAG_DOM_FILE ||
             aTag == SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE);

  int64_t lastModifiedDate;
  if (aTag == SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE) {
    lastModifiedDate = INT64_MAX;
  } else {
    if (NS_WARN_IF(!JS_ReadBytes(aReader, &lastModifiedDate,
                                 sizeof(lastModifiedDate)))) {
      return false;
    }
    lastModifiedDate = NativeEndian::swapFromLittleEndian(lastModifiedDate);
  }

  aRetval->lastModifiedDate = lastModifiedDate;

  nsCString name;
  if (NS_WARN_IF(!StructuredCloneReadString(aReader, name))) {
    return false;
  }

  CopyUTF8toUTF16(name, aRetval->name);

  return true;
}

bool ReadWasmModule(JSStructuredCloneReader* aReader, WasmModuleData* aRetval) {
  static_assert(SCTAG_DOM_WASM_MODULE == 0xFFFF8006, "Update me!");
  MOZ_ASSERT(aReader && aRetval);

  uint32_t bytecodeIndex;
  uint32_t compiledIndex;
  if (NS_WARN_IF(!JS_ReadUint32Pair(aReader, &bytecodeIndex, &compiledIndex))) {
    return false;
  }

  aRetval->bytecodeIndex = bytecodeIndex;
  aRetval->compiledIndex = compiledIndex;

  return true;
}

template <typename StructuredCloneFile>
class ValueDeserializationHelper;

class ValueDeserializationHelperBase {
 public:
  static bool CreateAndWrapWasmModule(JSContext* aCx,
                                      const StructuredCloneFileBase& aFile,
                                      const WasmModuleData& aData,
                                      JS::MutableHandle<JSObject*> aResult) {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aFile.Type() == StructuredCloneFileBase::eWasmBytecode);

    // Both on the parent and child side, just create a plain object here,
    // support for de-serialization of WebAssembly.Modules has been removed in
    // bug 1561876. Full removal is tracked in bug 1487479.

    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!obj)) {
      return false;
    }

    aResult.set(obj);
    return true;
  }

  template <typename StructuredCloneFile>
  static bool CreateAndWrapBlobOrFile(JSContext* aCx, IDBDatabase* aDatabase,
                                      const StructuredCloneFile& aFile,
                                      const BlobOrFileData& aData,
                                      JS::MutableHandle<JSObject*> aResult) {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aData.tag == SCTAG_DOM_FILE ||
               aData.tag == SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE ||
               aData.tag == SCTAG_DOM_BLOB);
    MOZ_ASSERT(aFile.Type() == StructuredCloneFileBase::eBlob);

    const auto blob = ValueDeserializationHelper<StructuredCloneFile>::GetBlob(
        aCx, aDatabase, aFile);
    if (NS_WARN_IF(!blob)) {
      return false;
    }

    if (aData.tag == SCTAG_DOM_BLOB) {
      blob->Impl()->SetLazyData(VoidString(), aData.type, aData.size,
                                INT64_MAX);
      MOZ_ASSERT(!blob->IsFile());

      // XXX The comment below is somewhat confusing, since it seems to imply
      // that this branch is only executed when called from ActorsParent, but
      // it's executed from both the parent and the child side code.

      // ActorsParent sends here a kind of half blob and half file wrapped into
      // a DOM File object. DOM File and DOM Blob are a WebIDL wrapper around a
      // BlobImpl object. SetLazyData() has just changed the BlobImpl to be a
      // Blob (see the previous assert), but 'blob' still has the WebIDL DOM
      // File wrapping.
      // Before exposing it to content, we must recreate a DOM Blob object.

      const RefPtr<Blob> exposedBlob =
          Blob::Create(blob->GetParentObject(), blob->Impl());
      if (NS_WARN_IF(!exposedBlob)) {
        return false;
      }

      return WrapAsJSObject(aCx, exposedBlob, aResult);
    }

    blob->Impl()->SetLazyData(aData.name, aData.type, aData.size,
                              aData.lastModifiedDate * PR_USEC_PER_MSEC);

    MOZ_ASSERT(blob->IsFile());
    const RefPtr<File> file = blob->ToFile();
    MOZ_ASSERT(file);

    return WrapAsJSObject(aCx, file, aResult);
  }
};

template <>
class ValueDeserializationHelper<StructuredCloneFileParent>
    : public ValueDeserializationHelperBase {
 public:
  static bool CreateAndWrapMutableFile(JSContext* aCx,
                                       StructuredCloneFileParent& aFile,
                                       const MutableFileData& aData,
                                       JS::MutableHandle<JSObject*> aResult) {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aFile.Type() == StructuredCloneFileBase::eBlob);

    // We are in an IDB SQLite schema upgrade where we don't care about a real
    // 'MutableFile', but we just care of having a proper |mType| flag.

    aFile.MutateType(StructuredCloneFileBase::eMutableFile);

    // Just make a dummy object.
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

    if (NS_WARN_IF(!obj)) {
      return false;
    }

    aResult.set(obj);
    return true;
  }

  static RefPtr<Blob> GetBlob(JSContext* aCx, IDBDatabase* aDatabase,
                              const StructuredCloneFileParent& aFile) {
    // This is chrome code, so there is no parent, but still we want to set a
    // correct parent for the new File object.
    const auto global = [aDatabase, aCx]() -> nsCOMPtr<nsIGlobalObject> {
      if (NS_IsMainThread()) {
        if (aDatabase && aDatabase->GetParentObject()) {
          return aDatabase->GetParentObject();
        }
        return xpc::CurrentNativeGlobal(aCx);
      }
      const WorkerPrivate* const workerPrivate =
          GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      WorkerGlobalScope* const globalScope = workerPrivate->GlobalScope();
      MOZ_ASSERT(globalScope);

      return do_QueryObject(globalScope);
    }();

    MOZ_ASSERT(global);

    // We do not have an mBlob but do have a DatabaseFileInfo.
    //
    // If we are creating an index, we do need a real-looking Blob/File instance
    // because the index's key path can reference their properties.  Rather than
    // create a fake-looking object, create a real Blob.
    //
    // If we are in a schema upgrade, we don't strictly need that, but we do not
    // need to optimize for that, and create it anyway.
    const nsCOMPtr<nsIFile> file = aFile.FileInfo().GetFileForFileInfo();
    if (!file) {
      return nullptr;
    }

    const auto impl = MakeRefPtr<FileBlobImpl>(file);
    impl->SetFileId(aFile.FileInfo().Id());
    return File::Create(global, impl);
  }
};

template <>
class ValueDeserializationHelper<StructuredCloneFileChild>
    : public ValueDeserializationHelperBase {
 public:
  static bool CreateAndWrapMutableFile(JSContext* aCx,
                                       StructuredCloneFileChild& aFile,
                                       const MutableFileData& aData,
                                       JS::MutableHandle<JSObject*> aResult) {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aFile.Type() == StructuredCloneFileBase::eMutableFile);

    return false;
  }

  static RefPtr<Blob> GetBlob(JSContext* aCx, IDBDatabase* aDatabase,
                              const StructuredCloneFileChild& aFile) {
    if (aFile.HasBlob()) {
      return aFile.BlobPtr();
    }

    MOZ_CRASH("Expected a StructuredCloneFile with a Blob");
  }
};

}  // namespace

template <typename StructuredCloneReadInfo>
JSObject* CommonStructuredCloneReadCallback(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aData,
    StructuredCloneReadInfo* aCloneReadInfo, IDBDatabase* aDatabase) {
  // We need to statically assert that our tag values are what we expect
  // so that if people accidentally change them they notice.
  static_assert(SCTAG_DOM_BLOB == 0xffff8001 &&
                    SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE == 0xffff8002 &&
                    SCTAG_DOM_MUTABLEFILE == 0xffff8004 &&
                    SCTAG_DOM_FILE == 0xffff8005 &&
                    SCTAG_DOM_WASM_MODULE == 0xffff8006,
                "You changed our structured clone tag values and just ate "
                "everyone's IndexedDB data.  I hope you are happy.");

  using StructuredCloneFile =
      typename StructuredCloneReadInfo::StructuredCloneFile;

  if (aTag == SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE ||
      aTag == SCTAG_DOM_BLOB || aTag == SCTAG_DOM_FILE ||
      aTag == SCTAG_DOM_MUTABLEFILE || aTag == SCTAG_DOM_WASM_MODULE) {
    JS::Rooted<JSObject*> result(aCx);

    if (aTag == SCTAG_DOM_WASM_MODULE) {
      WasmModuleData data(aData);
      if (NS_WARN_IF(!ReadWasmModule(aReader, &data))) {
        return nullptr;
      }

      MOZ_ASSERT(data.compiledIndex == data.bytecodeIndex + 1);
      MOZ_ASSERT(!data.flags);

      const auto& files = aCloneReadInfo->Files();
      if (data.bytecodeIndex >= files.Length() ||
          data.compiledIndex >= files.Length()) {
        MOZ_ASSERT(false, "Bad index value!");
        return nullptr;
      }

      const auto& file = files[data.bytecodeIndex];

      if (NS_WARN_IF(!ValueDeserializationHelper<StructuredCloneFile>::
                         CreateAndWrapWasmModule(aCx, file, data, &result))) {
        return nullptr;
      }

      return result;
    }

    if (aData >= aCloneReadInfo->Files().Length()) {
      MOZ_ASSERT(false, "Bad index value!");
      return nullptr;
    }

    auto& file = aCloneReadInfo->MutableFile(aData);

    if (aTag == SCTAG_DOM_MUTABLEFILE) {
      MutableFileData data;
      if (NS_WARN_IF(!ReadFileHandle(aReader, &data))) {
        return nullptr;
      }

      if (NS_WARN_IF(!ValueDeserializationHelper<StructuredCloneFile>::
                         CreateAndWrapMutableFile(aCx, file, data, &result))) {
        return nullptr;
      }

      return result;
    }

    BlobOrFileData data;
    if (NS_WARN_IF(!ReadBlobOrFile(aReader, aTag, &data))) {
      return nullptr;
    }

    if (NS_WARN_IF(!ValueDeserializationHelper<
                   StructuredCloneFile>::CreateAndWrapBlobOrFile(aCx, aDatabase,
                                                                 file, data,
                                                                 &result))) {
      return nullptr;
    }

    return result;
  }

  return StructuredCloneHolder::ReadFullySerializableObjects(aCx, aReader, aTag,
                                                             true);
}

template JSObject* CommonStructuredCloneReadCallback(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aData,
    StructuredCloneReadInfoChild* aCloneReadInfo, IDBDatabase* aDatabase);

template JSObject* CommonStructuredCloneReadCallback(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aData,
    StructuredCloneReadInfoParent* aCloneReadInfo, IDBDatabase* aDatabase);
}  // namespace mozilla::dom::indexedDB
