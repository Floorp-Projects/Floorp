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
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla::dom::fs {

namespace {

template <IterableIteratorBase::IteratorType Type>
struct ValueResolver;

template <>
struct ValueResolver<IterableIteratorBase::Keys> {
  void operator()(nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
                  const FileSystemEntryMetadata& aValue,
                  const RefPtr<Promise>& aPromise) {
    aPromise->MaybeResolve(aValue.entryName());
  }
};

template <>
struct ValueResolver<IterableIteratorBase::Values> {
  void operator()(nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
                  const FileSystemEntryMetadata& aValue,
                  const RefPtr<Promise>& aPromise) {
    RefPtr<FileSystemHandle> handle;

    if (aValue.directory()) {
      handle = new FileSystemDirectoryHandle(aGlobal, aManager, aValue);
    } else {
      handle = new FileSystemFileHandle(aGlobal, aManager, aValue);
    }

    aPromise->MaybeResolve(std::move(handle));
  }
};

template <>
struct ValueResolver<IterableIteratorBase::Entries> {
  void operator()(nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
                  const FileSystemEntryMetadata& aValue,
                  const RefPtr<Promise>& aPromise) {
    RefPtr<FileSystemHandle> handle;

    if (aValue.directory()) {
      handle = new FileSystemDirectoryHandle(aGlobal, aManager, aValue);
    } else {
      handle = new FileSystemFileHandle(aGlobal, aManager, aValue);
    }

    iterator_utils::ResolvePromiseWithKeyAndValue(aPromise, aValue.entryName(),
                                                  handle);
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
        mWithinPageEnd(0u),
        mWithinPageIndex(0u),
        mCurrentPageIsLastPage(true),
        mPageNumber(0u) {}

  // XXX This doesn't have to be public
  void ResolveValue(nsIGlobalObject* aGlobal,
                    RefPtr<FileSystemManager>& aManager,
                    const Maybe<DataType>& aValue, RefPtr<Promise> aPromise) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aPromise.get());

    if (!aValue) {
      iterator_utils::ResolvePromiseForFinished(aPromise);
      return;
    }

    ValueResolver{}(aGlobal, aManager, *aValue, aPromise);
  }

  already_AddRefed<Promise> Next(nsIGlobalObject* aGlobal,
                                 RefPtr<FileSystemManager>& aManager,
                                 ErrorResult& aError) override {
    RefPtr<Promise> promise = Promise::Create(aGlobal, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    next(aGlobal, aManager, promise, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    return promise.forget();
  }

 protected:
  ~DoubleBufferQueueImpl() override = default;

  void next(nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
            RefPtr<Promise> aResult, ErrorResult& aError) {
    LOG_VERBOSE(("next"));
    MOZ_ASSERT(aResult);

    Maybe<DataType> rawValue;

    // TODO: Would it be better to prefetch the items before
    // we hit the end of a page?
    // How likely it is that it would that lead to wasted fetch operations?
    if (0u == mWithinPageIndex) {
      RefPtr<Promise> promise = Promise::Create(aGlobal, aError);
      if (aError.Failed()) {
        return;
      }

      auto newPage = MakeRefPtr<FileSystemEntryMetadataArray>();

      promise->AddCallbacksWithCycleCollectedArgs(
          [self = RefPtr(this), newPage](
              JSContext*, JS::Handle<JS::Value>, ErrorResult&,
              RefPtr<FileSystemManager>& aManager, RefPtr<Promise>& aResult) {
            MOZ_ASSERT(0u == self->mWithinPageIndex);
            MOZ_ASSERT(newPage->Length() <= PageSize);

            const size_t startPos =
                self->mCurrentPageIsLastPage ? 0u : PageSize;
            if (self->mData.Length() < 2 * PageSize) {
              self->mData.InsertElementsAt(startPos, newPage->Elements(),
                                           newPage->Length());
            } else {
              self->mData.ReplaceElementsAt(startPos, newPage->Length(),
                                            newPage->Elements(),
                                            newPage->Length());
            }
            MOZ_ASSERT(self->mData.Length() <= 2 * PageSize);
            self->mWithinPageEnd = newPage->Length();

            Maybe<DataType> value;
            if (0 != newPage->Length()) {
              self->nextInternal(value);
            }

            self->ResolveValue(aResult->GetGlobalObject(), aManager, value,
                               aResult);
          },
          [](JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&,
             RefPtr<FileSystemManager>&,
             RefPtr<Promise>& aResult) { aResult->MaybeReject(aValue); },
          aManager, aResult);

      FileSystemRequestHandler{}.GetEntries(aManager, mEntryId, mPageNumber,
                                            promise, newPage, aError);
      if (aError.Failed()) {
        return;
      }

      ++mPageNumber;
      return;
    }

    nextInternal(rawValue);

    ResolveValue(aGlobal, aManager, rawValue, aResult);
  }

  bool nextInternal(Maybe<DataType>& aNext) {
    if (mWithinPageIndex >= mWithinPageEnd) {
      return false;
    }

    const size_t previous =
        mWithinPageIndex + (mCurrentPageIsLastPage ? 0 : PageSize);
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

already_AddRefed<mozilla::dom::FileSystemDirectoryIterator::Impl>
FileSystemDirectoryIteratorFactory::Create(
    const FileSystemEntryMetadata& aMetadata,
    IterableIteratorBase::IteratorType aType) {
  if (IterableIteratorBase::Entries == aType) {
    return MakeAndAddRef<UnderlyingQueue<IterableIteratorBase::Entries>>(
        aMetadata);
  }

  if (IterableIteratorBase::Values == aType) {
    return MakeAndAddRef<UnderlyingQueue<IterableIteratorBase::Values>>(
        aMetadata);
  }

  return MakeAndAddRef<UnderlyingQueue<IterableIteratorBase::Keys>>(aMetadata);
}

}  // namespace mozilla::dom::fs
