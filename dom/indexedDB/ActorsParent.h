/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorsparent_h__
#define mozilla_dom_indexeddb_actorsparent_h__

template <class> struct already_AddRefed;
class nsCString;
class nsIPrincipal;
class nsPIDOMWindow;

namespace mozilla {
namespace ipc {

class PBackgroundParent;

} // namespace ipc

namespace dom {

class TabParent;

namespace quota {

class Client;

} // namespace quota

namespace indexedDB {

class OptionalWindowId;
class PBackgroundIDBFactoryParent;
class PIndexedDBPermissionRequestParent;

PBackgroundIDBFactoryParent*
AllocPBackgroundIDBFactoryParent(mozilla::ipc::PBackgroundParent* aManager,
                                 const OptionalWindowId& aOptionalWindowId);

bool
RecvPBackgroundIDBFactoryConstructor(mozilla::ipc::PBackgroundParent* aManager,
                                     PBackgroundIDBFactoryParent* aActor,
                                     const OptionalWindowId& aOptionalWindowId);

bool
DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor);

PIndexedDBPermissionRequestParent*
AllocPIndexedDBPermissionRequestParent(nsPIDOMWindow* aWindow,
                                       nsIPrincipal* aPrincipal);

bool
RecvPIndexedDBPermissionRequestConstructor(
                                     PIndexedDBPermissionRequestParent* aActor);

bool
DeallocPIndexedDBPermissionRequestParent(
                                     PIndexedDBPermissionRequestParent* aActor);

already_AddRefed<mozilla::dom::quota::Client>
CreateQuotaClient();

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_actorsparent_h__
