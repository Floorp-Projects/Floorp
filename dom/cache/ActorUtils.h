/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_ActorUtils_h
#define mozilla_dom_cache_ActorUtils_h

#include "mozilla/dom/cache/Types.h"

namespace mozilla {

namespace ipc {
class PBackgroundParent;
class PrincipalInfo;
} // namespace ipc

namespace dom {
namespace cache {

class PCacheChild;
class PCacheParent;
class PCacheStreamControlChild;
class PCacheStreamControlParent;
class PCacheStorageChild;
class PCacheStorageParent;

// Factory methods for use in ipc/glue methods.  Implemented in individual actor
// cpp files.

PCacheChild*
AllocPCacheChild();

void
DeallocPCacheChild(PCacheChild* aActor);

void
DeallocPCacheParent(PCacheParent* aActor);

PCacheStreamControlChild*
AllocPCacheStreamControlChild();

void
DeallocPCacheStreamControlChild(PCacheStreamControlChild* aActor);

void
DeallocPCacheStreamControlParent(PCacheStreamControlParent* aActor);

PCacheStorageParent*
AllocPCacheStorageParent(mozilla::ipc::PBackgroundParent* aManagingActor,
                         Namespace aNamespace,
                         const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

void
DeallocPCacheStorageChild(PCacheStorageChild* aActor);

void
DeallocPCacheStorageParent(PCacheStorageParent* aActor);

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_ActorUtils_h
