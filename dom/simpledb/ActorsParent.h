/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_ActorsParent_h
#define mozilla_dom_simpledb_ActorsParent_h

#include "mozilla/dom/quota/PersistenceType.h"

template <class>
struct already_AddRefed;

namespace mozilla {

namespace ipc {

class PrincipalInfo;

}  // namespace ipc

namespace dom {

class PBackgroundSDBConnectionParent;

namespace quota {

class Client;

}  // namespace quota

already_AddRefed<PBackgroundSDBConnectionParent>
AllocPBackgroundSDBConnectionParent(
    const mozilla::dom::quota::PersistenceType& aPersistenceType,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

bool RecvPBackgroundSDBConnectionConstructor(
    PBackgroundSDBConnectionParent* aActor,
    const mozilla::dom::quota::PersistenceType& aPersistenceType,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

namespace simpledb {

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient();

}  // namespace simpledb

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_ActorsParent_h
