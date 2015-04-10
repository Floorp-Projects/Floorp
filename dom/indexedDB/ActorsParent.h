/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorsparent_h__
#define mozilla_dom_indexeddb_actorsparent_h__

template <class> struct already_AddRefed;
class nsCString;
struct nsID;
class nsIPrincipal;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Element;
class TabParent;

namespace quota {

class Client;

} // namespace quota

namespace indexedDB {

class LoggingInfo;
class PBackgroundIDBFactoryParent;
class PIndexedDBPermissionRequestParent;

PBackgroundIDBFactoryParent*
AllocPBackgroundIDBFactoryParent(const LoggingInfo& aLoggingInfo);

bool
RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                     const LoggingInfo& aLoggingInfo);

bool
DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor);

PIndexedDBPermissionRequestParent*
AllocPIndexedDBPermissionRequestParent(Element* aOwnerElement,
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
