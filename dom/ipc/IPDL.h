/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IPDL__
#define mozilla_dom_IPDL__

#include "nsClassHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsRefPtrHashtable.h"
#include "nsDataHashtable.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"

class nsIGlobalObject;

namespace mozilla {

namespace ipdl {
class IPDLProtocol;
class IPDLProtocolInstance;
enum class IPDLSide : bool;

namespace ipc {
class PContentParentIPCInterface;
class PContentChildIPCInterface;
} // ipc
} // ipdl

namespace dom {

class ContentParent;

class IPDL
  : public nsIObserver
  , public nsWrapperCache
{
protected:
  virtual ~IPDL();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IPDL)
  NS_DECL_NSIOBSERVER

  explicit IPDL(nsIGlobalObject* aParentObject);

  static already_AddRefed<IPDL> Constructor(const GlobalObject& aGlobal,
                                            ErrorResult& aRv);

  void SetupContentParent();

  void RegisterProtocol(JSContext* aCx, const nsAString& aProtocolName, const nsAString& aProtocolURI, bool aEagerLoad);

  void RegisterTopLevelClass(JSContext* aCx,
                             JS::Handle<JSObject*> aClassConstructor);
  void CreateTopLevelInstance(JSContext* aCx,
                              const Sequence<JS::Handle<JS::Value>>& aArgs);
  void GetTopLevelInstance(JSContext* aCx,
                           const Optional<uint32_t>& aId,
                           JS::MutableHandle<JSObject*> aRetval);
  void GetTopLevelInstances(JSContext* aCx,
                            nsTArray<JSObject*>& aRetval);

  bool DoResolve(JSContext* aCx,
                 JS::Handle<JSObject*> aObj,
                 JS::Handle<jsid> aId,
                 JS::MutableHandle<JS::PropertyDescriptor> aRv);

  void GetOwnPropertyNames(JSContext* aCx,
                           JS::AutoIdVector& aNames,
                           bool aEnumerableOnly,
                           ErrorResult& aRv);

  nsISupports* GetParentObject();

  nsIGlobalObject* GetGlobalObject() { return mParentObject; }

  static bool MayResolve(jsid aId);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::HandleObject aGivenProto) override;

  ipdl::ipc::PContentParentIPCInterface* GetParentInterface(uint32_t aId)
  {
    return mParentInterfaces.Get(aId);
  }

  ipdl::ipc::PContentChildIPCInterface* GetChildInterface()
  {
    return mChildInterface.get();
  }

  JSObject* NewTopLevelProtocolParent(
    JSContext* aCx,
    const Sequence<JS::Handle<JS::Value>>& aArgs,
    ContentParent* aCp);
  JSObject* NewTopLevelProtocolChild(
    JSContext* aCx,
    const Sequence<JS::Handle<JS::Value>>& aArgs);

protected:
  bool IsRealProperty(const nsAString& aPropertyName);

  bool ResolveProtocolProperty(const nsString& aProtocolPropertyName,
                               JS::Handle<JSObject*> aObj,
                               JSContext* aCx,
                               JS::MutableHandle<JS::PropertyDescriptor> aRv);

  Maybe<ipdl::IPDLSide> GetProtocolSide(const nsAString& aProtocolName);

  void StripParentSuffix(const nsAString& aProtocolName, nsAString& aRetVal);
  void StripChildSuffix(const nsAString& aProtocolName, nsAString& aRetVal);

  ipdl::ipc::PContentChildIPCInterface* GetOrInitChildInterface();

  already_AddRefed<ipdl::IPDLProtocol> NewIPDLProtocol(
    const nsAString& aProtocolName,
    JS::Handle<JSObject*> aObj,
    JSContext* aCx);

  typedef nsRefPtrHashtable<nsStringHashKey, ipdl::IPDLProtocol>
    ProtocolClassTable;

  ProtocolClassTable mParsedProtocolClassTable;
  nsTArray<nsString> mParsedProtocolIDs;
  nsIGlobalObject* MOZ_NON_OWNING_REF mParentObject;

  nsClassHashtable<nsUint32HashKey, ipdl::ipc::PContentParentIPCInterface>
    mParentInterfaces;
  UniquePtr<ipdl::ipc::PContentChildIPCInterface> mChildInterface;

  nsRefPtrHashtable<nsUint32HashKey, ipdl::IPDLProtocolInstance>
    mTopLevelParentInstances;
  RefPtr<ipdl::IPDLProtocolInstance> mTopLevelChildInstance;

  JS::Heap<JSObject*> mTopLevelClassConstructor;

  nsDataHashtable<nsStringHashKey, nsString> mProtocolURIs;

  static constexpr const nsLiteralString sPropertyNames[] = {NS_LITERAL_STRING("registerTopLevelClass"), NS_LITERAL_STRING("getTopLevelInstance"), NS_LITERAL_STRING("getTopLevelInstances"), NS_LITERAL_STRING("registerProtocol")};
  static constexpr size_t sPropertyNamesLength = ArrayLength(sPropertyNames);

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IPDL__
