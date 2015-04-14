/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/StreamUtils.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/CacheStreamControlChild.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"

namespace mozilla {
namespace dom {
namespace cache {

namespace {

using mozilla::unused;
using mozilla::void_t;
using mozilla::dom::cache::CacheStreamControlChild;
using mozilla::dom::cache::Feature;
using mozilla::dom::cache::CacheReadStream;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetChild;
using mozilla::ipc::OptionalFileDescriptorSet;

void
StartDestroyStreamChild(const CacheReadStream& aReadStream)
{
  CacheStreamControlChild* cacheControl =
    static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
  if (cacheControl) {
    cacheControl->StartDestroy();
  }

  if (aReadStream.fds().type() ==
      OptionalFileDescriptorSet::TPFileDescriptorSetChild) {
    nsAutoTArray<FileDescriptor, 4> fds;

    FileDescriptorSetChild* fdSetActor =
      static_cast<FileDescriptorSetChild*>(aReadStream.fds().get_PFileDescriptorSetChild());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    unused << fdSetActor->Send__delete__(fdSetActor);
  }
}

void
AddFeatureToStreamChild(const CacheReadStream& aReadStream, Feature* aFeature)
{
  CacheStreamControlChild* cacheControl =
    static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
  if (cacheControl) {
    cacheControl->SetFeature(aFeature);
  }
}

} // anonymous namespace

void
StartDestroyStreamChild(const CacheResponseOrVoid& aResponseOrVoid)
{
  if (aResponseOrVoid.type() == CacheResponseOrVoid::Tvoid_t) {
    return;
  }

  StartDestroyStreamChild(aResponseOrVoid.get_CacheResponse());
}

void
StartDestroyStreamChild(const CacheResponse& aResponse)
{
  if (aResponse.body().type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  StartDestroyStreamChild(aResponse.body().get_CacheReadStream());
}

void
StartDestroyStreamChild(const nsTArray<CacheResponse>& aResponses)
{
  for (uint32_t i = 0; i < aResponses.Length(); ++i) {
    StartDestroyStreamChild(aResponses[i]);
  }
}

void
StartDestroyStreamChild(const nsTArray<CacheRequest>& aRequests)
{
  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    if (aRequests[i].body().type() == CacheReadStreamOrVoid::Tvoid_t) {
      continue;
    }
    StartDestroyStreamChild(aRequests[i].body().get_CacheReadStream());
  }
}

void
AddFeatureToStreamChild(const CacheResponseOrVoid& aResponseOrVoid,
                        Feature* aFeature)
{
  if (aResponseOrVoid.type() == CacheResponseOrVoid::Tvoid_t) {
    return;
  }

  AddFeatureToStreamChild(aResponseOrVoid.get_CacheResponse(), aFeature);
}

void
AddFeatureToStreamChild(const CacheResponse& aResponse,
                        Feature* aFeature)
{
  if (aResponse.body().type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  AddFeatureToStreamChild(aResponse.body().get_CacheReadStream(), aFeature);
}

void
AddFeatureToStreamChild(const nsTArray<CacheResponse>& aResponses,
                         Feature* aFeature)
{
  for (uint32_t i = 0; i < aResponses.Length(); ++i) {
    AddFeatureToStreamChild(aResponses[i], aFeature);
  }
}

void
AddFeatureToStreamChild(const nsTArray<CacheRequest>& aRequests,
                         Feature* aFeature)
{
  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    if (aRequests[i].body().type() == CacheReadStreamOrVoid::Tvoid_t) {
      continue;
    }
    AddFeatureToStreamChild(aRequests[i].body().get_CacheReadStream(),
                            aFeature);
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
