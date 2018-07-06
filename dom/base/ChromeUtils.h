/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeUtils__
#define mozilla_dom_ChromeUtils__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {

namespace devtools {
class HeapSnapshot;
} // namespace devtools

namespace dom {

class ArrayBufferViewOrArrayBuffer;
class IdleRequestCallback;
struct IdleRequestOptions;
class MozQueryInterface;
class PrecompiledScript;
class Promise;

class ChromeUtils
{
private:
  // Implemented in devtools/shared/heapsnapshot/HeapSnapshot.cpp
  static void SaveHeapSnapshotShared(GlobalObject& global,
                                     const HeapSnapshotBoundaries& boundaries,
                                     nsAString& filePath,
                                     nsAString& snapshotId,
                                     ErrorResult& rv);

public:
  // Implemented in devtools/shared/heapsnapshot/HeapSnapshot.cpp
  static void SaveHeapSnapshot(GlobalObject& global,
                               const HeapSnapshotBoundaries& boundaries,
                               nsAString& filePath,
                               ErrorResult& rv);

  // Implemented in devtools/shared/heapsnapshot/HeapSnapshot.cpp
  static void SaveHeapSnapshotGetId(GlobalObject& global,
                                    const HeapSnapshotBoundaries& boundaries,
                                    nsAString& snapshotId,
                                    ErrorResult& rv);

  // Implemented in devtools/shared/heapsnapshot/HeapSnapshot.cpp
  static already_AddRefed<devtools::HeapSnapshot> ReadHeapSnapshot(GlobalObject& global,
                                                                   const nsAString& filePath,
                                                                   ErrorResult& rv);

  static void NondeterministicGetWeakMapKeys(GlobalObject& aGlobal,
                                             JS::Handle<JS::Value> aMap,
                                             JS::MutableHandle<JS::Value> aRetval,
                                             ErrorResult& aRv);

  static void NondeterministicGetWeakSetKeys(GlobalObject& aGlobal,
                                             JS::Handle<JS::Value> aSet,
                                             JS::MutableHandle<JS::Value> aRetval,
                                             ErrorResult& aRv);

  static void Base64URLEncode(GlobalObject& aGlobal,
                              const ArrayBufferViewOrArrayBuffer& aSource,
                              const Base64URLEncodeOptions& aOptions,
                              nsACString& aResult,
                              ErrorResult& aRv);

  static void Base64URLDecode(GlobalObject& aGlobal,
                              const nsACString& aString,
                              const Base64URLDecodeOptions& aOptions,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv);

  static void
  OriginAttributesToSuffix(GlobalObject& aGlobal,
                           const dom::OriginAttributesDictionary& aAttrs,
                           nsCString& aSuffix);

  static bool
  OriginAttributesMatchPattern(dom::GlobalObject& aGlobal,
                               const dom::OriginAttributesDictionary& aAttrs,
                               const dom::OriginAttributesPatternDictionary& aPattern);

  static void
  CreateOriginAttributesFromOrigin(dom::GlobalObject& aGlobal,
                                   const nsAString& aOrigin,
                                   dom::OriginAttributesDictionary& aAttrs,
                                   ErrorResult& aRv);

  static void
  FillNonDefaultOriginAttributes(dom::GlobalObject& aGlobal,
                                 const dom::OriginAttributesDictionary& aAttrs,
                                 dom::OriginAttributesDictionary& aNewAttrs);

  static bool
  IsOriginAttributesEqual(dom::GlobalObject& aGlobal,
                          const dom::OriginAttributesDictionary& aA,
                          const dom::OriginAttributesDictionary& aB);

  static bool
  IsOriginAttributesEqual(const dom::OriginAttributesDictionary& aA,
                          const dom::OriginAttributesDictionary& aB);

  static bool
  IsOriginAttributesEqualIgnoringFPD(const dom::OriginAttributesDictionary& aA,
                                     const dom::OriginAttributesDictionary& aB)
  {
    return aA.mAppId == aB.mAppId &&
           aA.mInIsolatedMozBrowser == aB.mInIsolatedMozBrowser &&
           aA.mUserContextId == aB.mUserContextId &&
           aA.mPrivateBrowsingId == aB.mPrivateBrowsingId;
  }

  // Implemented in js/xpconnect/loader/ChromeScriptLoader.cpp
  static already_AddRefed<Promise>
  CompileScript(GlobalObject& aGlobal,
                const nsAString& aUrl,
                const dom::CompileScriptOptionsDictionary& aOptions,
                ErrorResult& aRv);

  static MozQueryInterface*
  GenerateQI(const GlobalObject& global, const Sequence<OwningStringOrIID>& interfaces,
             ErrorResult& aRv);

  static void WaiveXrays(GlobalObject& aGlobal,
                         JS::HandleValue aVal,
                         JS::MutableHandleValue aRetval,
                         ErrorResult& aRv);

  static void UnwaiveXrays(GlobalObject& aGlobal,
                           JS::HandleValue aVal,
                           JS::MutableHandleValue aRetval,
                           ErrorResult& aRv);

  static void GetClassName(GlobalObject& aGlobal,
                           JS::HandleObject aObj,
                           bool aUnwrap,
                           nsAString& aRetval);

  static void ShallowClone(GlobalObject& aGlobal,
                           JS::HandleObject aObj,
                           JS::HandleObject aTarget,
                           JS::MutableHandleObject aRetval,
                           ErrorResult& aRv);

  static void IdleDispatch(const GlobalObject& global,
                           IdleRequestCallback& callback,
                           const IdleRequestOptions& options,
                           ErrorResult& aRv);

  static void GetRecentJSDevError(GlobalObject& aGlobal,
                                  JS::MutableHandleValue aRetval,
                                  ErrorResult& aRv);

  static void ClearRecentJSDevError(GlobalObject& aGlobal);

  static void RequestPerformanceMetrics(GlobalObject& aGlobal);

  static void Import(const GlobalObject& aGlobal,
                     const nsAString& aResourceURI,
                     const Optional<JS::Handle<JSObject*>>& aTargetObj,
                     JS::MutableHandle<JSObject*> aRetval,
                     ErrorResult& aRv);

  static void DefineModuleGetter(const GlobalObject& global,
                                 JS::Handle<JSObject*> target,
                                 const nsAString& id,
                                 const nsAString& resourceURI,
                                 ErrorResult& aRv);

  static void
  GetCallerLocation(const GlobalObject& global, nsIPrincipal* principal,
                    JS::MutableHandle<JSObject*> aRetval);

  static void
  CreateError(const GlobalObject& global, const nsAString& message,
              JS::Handle<JSObject*> stack,
              JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv);

  static already_AddRefed<Promise>
  RequestIOActivity(GlobalObject& aGlobal, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChromeUtils__
