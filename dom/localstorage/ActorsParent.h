/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ActorsParent_h
#define mozilla_dom_localstorage_ActorsParent_h

namespace mozilla {

namespace ipc {

class PBackgroundParent;
class PrincipalInfo;

} // namespace ipc

namespace dom {

class LSRequestParams;
class PBackgroundLSObjectParent;
class PBackgroundLSObserverParent;
class PBackgroundLSRequestParent;

namespace quota {

class Client;

} // namespace quota

PBackgroundLSObjectParent*
AllocPBackgroundLSObjectParent(
                              const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                              const nsString& aDocumentURI,
                              const uint32_t& aPrivateBrowsingId);

bool
RecvPBackgroundLSObjectConstructor(
                              PBackgroundLSObjectParent* aActor,
                              const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                              const nsString& aDocumentURI,
                              const uint32_t& aPrivateBrowsingId);

bool
DeallocPBackgroundLSObjectParent(PBackgroundLSObjectParent* aActor);

PBackgroundLSObserverParent*
AllocPBackgroundLSObserverParent(const uint64_t& aObserverId);

bool
RecvPBackgroundLSObserverConstructor(PBackgroundLSObserverParent* aActor,
                                     const uint64_t& aObservereId);

bool
DeallocPBackgroundLSObserverParent(PBackgroundLSObserverParent* aActor);

PBackgroundLSRequestParent*
AllocPBackgroundLSRequestParent(
                              mozilla::ipc::PBackgroundParent* aBackgroundActor,
                              const LSRequestParams& aParams);

bool
RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                    const LSRequestParams& aParams);

bool
DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor);

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client>
CreateQuotaClient();

} // namespace localstorage

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_ActorsParent_h
