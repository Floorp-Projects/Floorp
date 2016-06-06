/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectoryReader.h"
#include "FileEntry.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

namespace {

class EmptyEntriesCallbackRunnable final : public Runnable
{
public:
  EmptyEntriesCallbackRunnable(EntriesCallback* aCallback)
    : mCallback(aCallback)
  {
    MOZ_ASSERT(aCallback);
  }

  NS_IMETHOD
  Run() override
  {
    Sequence<OwningNonNull<Entry>> sequence;
    mCallback->HandleEvent(sequence);
    return NS_OK;
  }

private:
  RefPtr<EntriesCallback> mCallback;
};

class PromiseHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  PromiseHandler(nsIGlobalObject* aGlobalObject,
                 EntriesCallback* aSuccessCallback,
                 ErrorCallback* aErrorCallback)
    : mGlobal(aGlobalObject)
    , mSuccessCallback(aSuccessCallback)
    , mErrorCallback(aErrorCallback)
  {
    MOZ_ASSERT(aGlobalObject);
    MOZ_ASSERT(aSuccessCallback);
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if(NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

    uint32_t length;
    if (NS_WARN_IF(!JS_GetArrayLength(aCx, obj, &length))) {
      return;
    }

    Sequence<OwningNonNull<Entry>> sequence;
    if (NS_WARN_IF(!sequence.SetLength(length, fallible))) {
      return;
    }

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, obj, i, &value))) {
        return;
      }

      if(NS_WARN_IF(!value.isObject())) {
        return;
      }

      JS::Rooted<JSObject*> valueObj(aCx, &value.toObject());

      RefPtr<File> file;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(File, valueObj, file))) {
        RefPtr<FileEntry> entry = new FileEntry(mGlobal, file);
        sequence[i] = entry;
        continue;
      }

      RefPtr<Directory> directory;
      if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Directory, valueObj,
                                             directory)))) {
        return;
      }

      RefPtr<DirectoryEntry> entry = new DirectoryEntry(mGlobal, directory);
      sequence[i] = entry;
    }

    mSuccessCallback->HandleEvent(sequence);
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (mErrorCallback) {
      RefPtr<ErrorCallbackRunnable> runnable =
        new ErrorCallbackRunnable(mGlobal, mErrorCallback,
                                  NS_ERROR_DOM_INVALID_STATE_ERR);
      nsresult rv = NS_DispatchToMainThread(runnable);
      NS_WARN_IF(NS_FAILED(rv));
    }
  }

private:
  ~PromiseHandler() {}

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<EntriesCallback> mSuccessCallback;
  RefPtr<ErrorCallback> mErrorCallback;
};

NS_IMPL_ISUPPORTS0(PromiseHandler);

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DirectoryReader, mParent, mDirectory)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DirectoryReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DirectoryReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DirectoryReader::DirectoryReader(nsIGlobalObject* aGlobal,
                                 Directory* aDirectory)
  : mParent(aGlobal)
  , mDirectory(aDirectory)
  , mAlreadyRead(false)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aDirectory);
}

DirectoryReader::~DirectoryReader()
{}

JSObject*
DirectoryReader::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DirectoryReaderBinding::Wrap(aCx, this, aGivenProto);
}

void
DirectoryReader::ReadEntries(EntriesCallback& aSuccessCallback,
                             const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                             ErrorResult& aRv)
{
  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
      new EmptyEntriesCallbackRunnable(&aSuccessCallback);
    aRv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(aRv.Failed());
    return;
  }

  // This object can be used only once.
  mAlreadyRead = true;

  ErrorResult rv;
  RefPtr<Promise> promise = mDirectory->GetFilesAndDirectories(rv);
  if (NS_WARN_IF(rv.Failed())) {
    if (aErrorCallback.WasPassed()) {
      RefPtr<ErrorCallbackRunnable> runnable =
        new ErrorCallbackRunnable(GetParentObject(),
                                  &aErrorCallback.Value(),
                                  rv.StealNSResult());
      aRv = NS_DispatchToMainThread(runnable);
      NS_WARN_IF(aRv.Failed());
    }

    return;
  }

  RefPtr<PromiseHandler> handler =
    new PromiseHandler(GetParentObject(), &aSuccessCallback,
                       aErrorCallback.WasPassed()
                         ? &aErrorCallback.Value() : nullptr);
  promise->AppendNativeHandler(handler);
}

} // dom namespace
} // mozilla namespace
