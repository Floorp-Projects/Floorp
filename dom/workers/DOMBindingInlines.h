/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_dombindinginlines_h__
#define mozilla_dom_workers_dombindinginlines_h__

#include "mozilla/dom/FileReaderSyncBinding.h"
#include "mozilla/dom/JSSlots.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"
#include "mozilla/dom/WorkerLocationBinding.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/dom/URLBinding.h"
#include "jsfriendapi.h"

BEGIN_WORKERS_NAMESPACE

class FileReaderSync;
class XMLHttpRequest;
class XMLHttpRequestUpload;
class WorkerLocation;
class WorkerNavigator;
class URL;

namespace {

template <class T>
struct WrapPrototypeTraits
{ };

// XXX I kinda hate this, but we decided it wasn't worth generating this in the
//     binding headers.
#define SPECIALIZE_PROTO_TRAITS(_class)                                        \
  template <>                                                                  \
  struct WrapPrototypeTraits<_class>                                           \
  {                                                                            \
    static inline JSClass*                                                     \
    GetJSClass()                                                               \
    {                                                                          \
      using namespace mozilla::dom;                                            \
      return _class##Binding_workers::GetJSClass();                            \
    }                                                                          \
                                                                               \
    static inline JSObject*                                                    \
    GetProtoObject(JSContext* aCx, JS::Handle<JSObject*> aGlobal)              \
    {                                                                          \
      using namespace mozilla::dom;                                            \
      return _class##Binding_workers::GetProtoObject(aCx, aGlobal);            \
    }                                                                          \
  };

SPECIALIZE_PROTO_TRAITS(FileReaderSync)
SPECIALIZE_PROTO_TRAITS(XMLHttpRequest)
SPECIALIZE_PROTO_TRAITS(XMLHttpRequestUpload)
SPECIALIZE_PROTO_TRAITS(WorkerLocation)
SPECIALIZE_PROTO_TRAITS(WorkerNavigator)
SPECIALIZE_PROTO_TRAITS(URL)

#undef SPECIALIZE_PROTO_TRAITS

} // anonymous namespace

template <class T>
inline JSObject*
Wrap(JSContext* aCx, JSObject* aGlobal, nsRefPtr<T>& aConcreteObject)
{
  MOZ_ASSERT(aCx);

  if (!aGlobal) {
    aGlobal = JS::CurrentGlobalOrNull(aCx);
    if (!aGlobal) {
      return NULL;
    }
  }

  JS::Rooted<JSObject*> global(aCx, aGlobal);
  JSObject* proto = WrapPrototypeTraits<T>::GetProtoObject(aCx, global);
  if (!proto) {
    return NULL;
  }

  JSObject* wrapper =
    JS_NewObject(aCx, WrapPrototypeTraits<T>::GetJSClass(), proto, global);
  if (!wrapper) {
    return NULL;
  }

  js::SetReservedSlot(wrapper, DOM_OBJECT_SLOT,
                      PRIVATE_TO_JSVAL(aConcreteObject));

  aConcreteObject->SetIsDOMBinding();
  aConcreteObject->SetWrapper(wrapper);

  NS_ADDREF(aConcreteObject.get());
  return wrapper;
}

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_dombindinginlines_h__
