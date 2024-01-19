/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ActorsParent_h
#define mozilla_dom_localstorage_ActorsParent_h

#include <cstdint>
#include "mozilla/AlreadyAddRefed.h"

namespace mozilla {

namespace ipc {

class PBackgroundParent;
class PrincipalInfo;

}  // namespace ipc

namespace dom {

class LSRequestParams;
class LSSimpleRequestParams;
class PBackgroundLSDatabaseParent;
class PBackgroundLSObserverParent;
class PBackgroundLSRequestParent;
class PBackgroundLSSimpleRequestParent;

namespace quota {

class Client;

}  // namespace quota

void InitializeLocalStorage();

already_AddRefed<PBackgroundLSDatabaseParent> AllocPBackgroundLSDatabaseParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const uint32_t& aPrivateBrowsingId, const uint64_t& aDatastoreId);

bool RecvPBackgroundLSDatabaseConstructor(
    PBackgroundLSDatabaseParent* aActor,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const uint32_t& aPrivateBrowsingId, const uint64_t& aDatastoreId);

PBackgroundLSObserverParent* AllocPBackgroundLSObserverParent(
    const uint64_t& aObserverId);

bool RecvPBackgroundLSObserverConstructor(PBackgroundLSObserverParent* aActor,
                                          const uint64_t& aObservereId);

bool DeallocPBackgroundLSObserverParent(PBackgroundLSObserverParent* aActor);

PBackgroundLSRequestParent* AllocPBackgroundLSRequestParent(
    mozilla::ipc::PBackgroundParent* aBackgroundActor,
    const LSRequestParams& aParams);

bool RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                         const LSRequestParams& aParams);

bool DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor);

PBackgroundLSSimpleRequestParent* AllocPBackgroundLSSimpleRequestParent(
    mozilla::ipc::PBackgroundParent* aBackgroundActor,
    const LSSimpleRequestParams& aParams);

bool RecvPBackgroundLSSimpleRequestConstructor(
    PBackgroundLSSimpleRequestParent* aActor,
    const LSSimpleRequestParams& aParams);

bool DeallocPBackgroundLSSimpleRequestParent(
    PBackgroundLSSimpleRequestParent* aActor);

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient();

}  // namespace localstorage

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_ActorsParent_h
