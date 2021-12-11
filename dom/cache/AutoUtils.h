/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_AutoUtils_h
#define mozilla_dom_cache_AutoUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "nsTArray.h"

struct nsID;

namespace mozilla {

class ErrorResult;

namespace ipc {
class PBackgroundParent;
class AutoIPCStream;
}  // namespace ipc

namespace dom {

class InternalRequest;

namespace cache {

class CacheStreamControlParent;
class Manager;
struct SavedRequest;
struct SavedResponse;
class StreamList;

// A collection of RAII-style helper classes to ensure that IPC
// FileDescriptorSet actors are properly cleaned up.  The user of these actors
// must manually either Forget() the Fds or Send__delete__() the actor
// depending on if the descriptors were actually sent.
//
// Note, these should only be used when *sending* streams across IPC.  The
// deserialization case is handled by creating a ReadStream object.

class MOZ_STACK_CLASS AutoChildOpArgs final {
 public:
  using BodyAction = TypeUtils::BodyAction;
  using SchemeAction = TypeUtils::SchemeAction;

  AutoChildOpArgs(TypeUtils* aTypeUtils, const CacheOpArgs& aOpArgs,
                  uint32_t aEntryCount);
  ~AutoChildOpArgs();

  void Add(const InternalRequest& aRequest, BodyAction aBodyAction,
           SchemeAction aSchemeAction, ErrorResult& aRv);
  void Add(JSContext* aCx, const InternalRequest& aRequest,
           BodyAction aBodyAction, SchemeAction aSchemeAction,
           Response& aResponse, ErrorResult& aRv);

  const CacheOpArgs& SendAsOpArgs();

 private:
  TypeUtils* mTypeUtils;
  CacheOpArgs mOpArgs;
  nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>> mStreamCleanupList;
  bool mSent;
};

class MOZ_STACK_CLASS AutoParentOpResult final {
 public:
  AutoParentOpResult(mozilla::ipc::PBackgroundParent* aManager,
                     const CacheOpResult& aOpResult, uint32_t aEntryCount);
  ~AutoParentOpResult();

  void Add(CacheId aOpenedCacheId, SafeRefPtr<Manager> aManager);
  void Add(const SavedResponse& aSavedResponse, StreamList& aStreamList);
  void Add(const SavedRequest& aSavedRequest, StreamList& aStreamList);

  const CacheOpResult& SendAsOpResult();

 private:
  void SerializeResponseBody(const SavedResponse& aSavedResponse,
                             StreamList& aStreamList,
                             CacheResponse* aResponseOut);

  void SerializeReadStream(const nsID& aId, StreamList& aStreamList,
                           CacheReadStream* aReadStreamOut);

  mozilla::ipc::PBackgroundParent* mManager;
  CacheOpResult mOpResult;
  CacheStreamControlParent* mStreamControl;
  nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>> mStreamCleanupList;
  bool mSent;
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_AutoUtils_h
