/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryIteratorFactory.h"

#include "FileSystemEntryMetadataArray.h"
#include "fs/FileSystemRequestHandler.h"
#include "jsapi.h"
#include "mozilla/dom/FileSystemDirectoryHandle.h"
#include "mozilla/dom/FileSystemDirectoryIterator.h"
#include "mozilla/dom/FileSystemFileHandle.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla::dom::fs {

namespace {

inline JSContext* GetContext(const RefPtr<Promise>& aPromise) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aPromise->GetGlobalObject()))) {
    return nullptr;
  }
  return jsapi.cx();
}

template <IterableIteratorBase::IteratorType Type>
struct ValueResolver;

template <>
struct ValueResolver<IterableIteratorBase::Keys> {
  nsresult operator()(nsIGlobalObject* aGlobal,
                      RefPtr<FileSystemManager>& aManager,
                      const FileSystemEntryMetadata& aValue,
                      const RefPtr<Promise>& aPromise, ErrorResult& aError) {
    JSContext* cx = GetContext(aPromise);
    if (!cx) {
      return NS_ERROR_UNEXPECTED;
    }

    JS::Rooted<JS::Value> key(cx);

    if (!ToJSValue(cx, aValue.entryName(), &key)) {
      return NS_ERROR_INVALID_ARG;
    }

    iterator_utils::ResolvePromiseWithKeyOrValue(cx, aPromise.get(), key,
                                                 aError);

    return NS_OK;
  }
};

template <>
struct ValueResolver<IterableIteratorBase::Values> {
  nsresult operator()(nsIGlobalObject* aGlobal,
                      RefPtr<FileSystemManager>& aManager,
                      const FileSystemEntryMetadata& aValue,
                      const RefPtr<Promise>& aPromise, ErrorResult& aError) {
    JSContext* cx = GetContext(aPromise);
    if (!cx) {
      return NS_ERROR_UNEXPECTED;
    }

    RefPtr<FileSystemHandle> handle;

    if (aValue.directory()) {
      handle = new FileSystemDirectoryHandle(aGlobal, aManager, aValue);
    } else {
      handle = new FileSystemFileHandle(aGlobal, aManager, aValue);
    }

    JS::Rooted<JS::Value> value(cx);
    if (!ToJSValue(cx, handle, &value)) {
      return NS_ERROR_INVALID_ARG;
    }

    iterator_utils::ResolvePromiseWithKeyOrValue(cx, aPromise.get(), value,
                                                 aError);

    return NS_OK;
  }
};

template <>
struct ValueResolver<IterableIteratorBase::Entries> {
  nsresult operator()(nsIGlobalObject* aGlobal,
                      RefPtr<FileSystemManager>& aManager,
                      const FileSystemEntryMetadata& aValue,
                      const RefPtr<Promise>& aPromise, ErrorResult& aError) {
    JSContext* cx = GetContext(aPromise);
    if (!cx) {
      return NS_ERROR_UNEXPECTED;
    }

    JS::Rooted<JS::Value> key(cx);
    if (!ToJSValue(cx, aValue.entryName(), &key)) {
      return NS_ERROR_INVALID_ARG;
    }

    RefPtr<FileSystemHandle> handle;

    if (aValue.directory()) {
      handle = new FileSystemDirectoryHandle(aGlobal, aManager, aValue);
    } else {
      handle = new FileSystemFileHandle(aGlobal, aManager, aValue);
    }

    JS::Rooted<JS::Value> value(cx);
    if (!ToJSValue(cx, handle, &value)) {
      return NS_ERROR_INVALID_ARG;
    }

    iterator_utils::ResolvePromiseWithKeyAndValue(cx, aPromise.get(), key,
                                                  value, aError);

    return NS_OK;
  }
};

// TODO: PageSize could be compile-time shared between content and parent
template <class ValueResolver, size_t PageSize = 1024u>
class DoubleBufferQueueImpl
    : public mozilla::dom::FileSystemDirectoryIterator::Impl {
  static_assert(PageSize > 0u);

 public:
  using DataType = FileSystemEntryMetadata;
  explicit DoubleBufferQueueImpl(const FileSystemEntryMetadata& aMetadata)
      : mEntryId(aMetadata.entryId()),
        mData(),
        mWithinPageEnd(0u),
        mWithinPageIndex(0u),
        mCurrentPageIsLastPage(true),
        mPageNumber(0u) {}

  // XXX This doesn't have to be public
  void ResolveValue(nsIGlobalObject* aGlobal,
                    RefPtr<FileSystemManager>& aManager,
                    const Maybe<DataType>& aValue, RefPtr<Promise> aPromise,
                    ErrorResult& aError) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aPromise.get());

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(aPromise->GetGlobalObject()))) {
      aPromise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
      aError = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    JSContext* aCx = jsapi.cx();
    MOZ_ASSERT(aCx);

    if (!aValue) {
      iterator_utils::ResolvePromiseForFinished(aCx, aPromise.get(), aError);
      return;
    }

    ValueResolver{}(aGlobal, aManager, *aValue, aPromise, aError);
  }

  already_AddRefed<Promise> Next(nsIGlobalObject* aGlobal,
                                 RefPtr<FileSystemManager>& aManager,
                                 ErrorResult& aError) override {
    RefPtr<Promise> promise = Promise::Create(aGlobal, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    next(aGlobal, aManager, promise, aError);

    return promise.forget();
  }

  ~DoubleBufferQueueImpl() = default;

 protected:
  void next(nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
            RefPtr<Promise> aResult, ErrorResult& aError) {
    MOZ_ASSERT(aResult);

    Maybe<DataType> rawValue;

    // TODO: Would it be better to prefetch the items before
    // we hit the end of a page?
    // How likely it is that it would that lead to wasted fetch operations?
    if (0u == mWithinPageIndex) {
      ErrorResult rv;
      RefPtr<Promise> promise = Promise::Create(aGlobal, rv);
      if (rv.Failed()) {
        aResult->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
        return;
      }

      auto newPage = MakeRefPtr<FileSystemEntryMetadataArray>();

      RefPtr<DomPromiseListener> listener = new DomPromiseListener(
          [global = nsCOMPtr<nsIGlobalObject>(aGlobal),
           manager = RefPtr<FileSystemManager>(aManager), newPage, aResult,
           this](JSContext* aCx, JS::Handle<JS::Value> aValue) mutable {
            MOZ_ASSERT(0u == mWithinPageIndex);

            // XXX Do we need this extra copy ?
            nsTArray<DataType> batch;
            for (const auto& it : *newPage) {
              batch.AppendElement(it);
            }

            const size_t batchSize = std::min(PageSize, newPage->Length());
            mData.InsertElementsAt(
                PageSize * static_cast<size_t>(!mCurrentPageIsLastPage),
                batch.Elements(), batchSize);
            mWithinPageEnd += batchSize;

            Maybe<DataType> value;
            if (0 != newPage->Length()) {
              nextInternal(value);
            }

            IgnoredErrorResult rv;
            ResolveValue(global, manager, value, aResult, rv);
          },
          [](nsresult aRv) {});
      promise->AppendNativeHandler(listener);

      FileSystemRequestHandler{}.GetEntries(aManager, mEntryId, mPageNumber,
                                            promise, newPage);

      ++mPageNumber;
      return;
    }

    nextInternal(rawValue);

    ResolveValue(aGlobal, aManager, rawValue, aResult, aError);
  }

  bool nextInternal(Maybe<DataType>& aNext) {
    if (mWithinPageIndex >= mWithinPageEnd) {
      return false;
    }

    const auto previous =
        static_cast<size_t>(!mCurrentPageIsLastPage) * PageSize +
        mWithinPageIndex;
    MOZ_ASSERT(2u * PageSize > previous);
    MOZ_ASSERT(previous < mData.Length());

    ++mWithinPageIndex;

    if (mWithinPageIndex == PageSize) {
      // Page end reached
      mWithinPageIndex = 0u;
      mCurrentPageIsLastPage = !mCurrentPageIsLastPage;
    }

    aNext = Some(mData[previous]);
    return true;
  }

  const EntryId mEntryId;

  nsTArray<DataType> mData;  // TODO: Fixed size above one page?

  size_t mWithinPageEnd = 0u;
  size_t mWithinPageIndex = 0u;
  bool mCurrentPageIsLastPage = true;  // In the beginning, first page is free
  PageNumber mPageNumber = 0u;
};

template <IterableIteratorBase::IteratorType Type>
using UnderlyingQueue = DoubleBufferQueueImpl<ValueResolver<Type>>;

}  // namespace

UniquePtr<mozilla::dom::FileSystemDirectoryIterator::Impl>
FileSystemDirectoryIteratorFactory::Create(
    const FileSystemEntryMetadata& aMetadata,
    IterableIteratorBase::IteratorType aType) {
  if (IterableIteratorBase::Entries == aType) {
    return MakeUnique<UnderlyingQueue<IterableIteratorBase::Entries>>(
        aMetadata);
  }

  if (IterableIteratorBase::Values == aType) {
    return MakeUnique<UnderlyingQueue<IterableIteratorBase::Values>>(aMetadata);
  }

  return MakeUnique<UnderlyingQueue<IterableIteratorBase::Keys>>(aMetadata);
}

}  // namespace mozilla::dom::fs
