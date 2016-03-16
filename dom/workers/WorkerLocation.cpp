/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WorkerLocation.h"

#include "mozilla/dom/WorkerLocationBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WorkerLocation)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerLocation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerLocation, Release)

/* static */ already_AddRefed<WorkerLocation>
WorkerLocation::Create(workers::WorkerPrivate::LocationInfo& aInfo)
{
  RefPtr<WorkerLocation> location =
    new WorkerLocation(NS_ConvertUTF8toUTF16(aInfo.mHref),
                       NS_ConvertUTF8toUTF16(aInfo.mProtocol),
                       NS_ConvertUTF8toUTF16(aInfo.mHost),
                       NS_ConvertUTF8toUTF16(aInfo.mHostname),
                       NS_ConvertUTF8toUTF16(aInfo.mPort),
                       NS_ConvertUTF8toUTF16(aInfo.mPathname),
                       NS_ConvertUTF8toUTF16(aInfo.mSearch),
                       NS_ConvertUTF8toUTF16(aInfo.mHash),
                       aInfo.mOrigin);

  return location.forget();
}

JSObject*
WorkerLocation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WorkerLocationBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
