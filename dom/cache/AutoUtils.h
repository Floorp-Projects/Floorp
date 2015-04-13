/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_AutoUtils_h
#define mozilla_dom_cache_AutoUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/PCacheTypes.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "nsTArray.h"

struct nsID;

namespace mozilla {

class ErrorResult;

namespace ipc {
class PBackgroundParent;
}

namespace dom {

class InternalRequest;

namespace cache {

class CacheStreamControlParent;
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

class MOZ_STACK_CLASS AutoChildBase
{
protected:
  typedef TypeUtils::BodyAction BodyAction;
  typedef TypeUtils::ReferrerAction ReferrerAction;
  typedef TypeUtils::SchemeAction SchemeAction;

  AutoChildBase(TypeUtils* aTypeUtils);
  virtual ~AutoChildBase() = 0;

  TypeUtils* mTypeUtils;
  bool mSent;
};

class MOZ_STACK_CLASS AutoChildRequest final : public AutoChildBase
{
public:
  explicit AutoChildRequest(TypeUtils* aTypeUtils);
  ~AutoChildRequest();

  void Add(InternalRequest* aRequest, BodyAction aBodyAction,
           ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
           ErrorResult& aRv);

  const PCacheRequest& SendAsRequest();
  const PCacheRequestOrVoid& SendAsRequestOrVoid();

private:
  PCacheRequestOrVoid mRequestOrVoid;
};

class MOZ_STACK_CLASS AutoChildRequestList final : public AutoChildBase
{
public:
  AutoChildRequestList(TypeUtils* aTypeUtils, uint32_t aCapacity);
  ~AutoChildRequestList();

  void Add(InternalRequest* aRequest, BodyAction aBodyAction,
           ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
           ErrorResult& aRv);

  const nsTArray<PCacheRequest>& SendAsRequestList();

private:
  // Allocates ~5k inline in the stack-only class
  nsAutoTArray<PCacheRequest, 32> mRequestList;
};

class MOZ_STACK_CLASS AutoChildRequestResponse final : public AutoChildBase
{
public:
  explicit AutoChildRequestResponse(TypeUtils* aTypeUtils);
  ~AutoChildRequestResponse();

  void Add(InternalRequest* aRequest, BodyAction aBodyAction,
           ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
           ErrorResult& aRv);
  void Add(Response& aResponse, ErrorResult& aRv);

  const CacheRequestResponse& SendAsRequestResponse();

private:
  CacheRequestResponse mRequestResponse;
};

class MOZ_STACK_CLASS AutoParentBase
{
protected:
  explicit AutoParentBase(mozilla::ipc::PBackgroundParent* aManager);
  virtual ~AutoParentBase() = 0;

  void SerializeReadStream(const nsID& aId, StreamList* aStreamList,
                           PCacheReadStream* aReadStreamOut);

  mozilla::ipc::PBackgroundParent* mManager;
  CacheStreamControlParent* mStreamControl;
  bool mSent;
};

class MOZ_STACK_CLASS AutoParentRequestList final : public AutoParentBase
{
public:
  AutoParentRequestList(mozilla::ipc::PBackgroundParent* aManager,
                        uint32_t aCapacity);
  ~AutoParentRequestList();

  void Add(const SavedRequest& aSavedRequest, StreamList* aStreamList);

  const nsTArray<PCacheRequest>& SendAsRequestList();

private:
  // Allocates ~5k inline in the stack-only class
  nsAutoTArray<PCacheRequest, 32> mRequestList;
};

class MOZ_STACK_CLASS AutoParentResponseList final : public AutoParentBase
{
public:
  AutoParentResponseList(mozilla::ipc::PBackgroundParent* aManager,
                         uint32_t aCapacity);
  ~AutoParentResponseList();

  void Add(const SavedResponse& aSavedResponse, StreamList* aStreamList);

  const nsTArray<PCacheResponse>& SendAsResponseList();

private:
  // Allocates ~4k inline in the stack-only class
  nsAutoTArray<PCacheResponse, 32> mResponseList;
};

class MOZ_STACK_CLASS AutoParentResponseOrVoid final : public AutoParentBase
{
public:
  explicit AutoParentResponseOrVoid(mozilla::ipc::PBackgroundParent* aManager);
  ~AutoParentResponseOrVoid();

  void Add(const SavedResponse& aSavedResponse, StreamList* aStreamList);

  const PCacheResponseOrVoid& SendAsResponseOrVoid();

private:
  PCacheResponseOrVoid mResponseOrVoid;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_AutoUtils_h
