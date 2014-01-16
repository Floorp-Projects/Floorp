/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAllCollection.h"

#include "mozilla/HoldDropJSObjects.h"
#include "nsDOMClassInfo.h"
#include "nsHTMLDocument.h"
#include "jsapi.h"
#include "nsWrapperCacheInlines.h"

namespace mozilla {
namespace dom {

HTMLAllCollection::HTMLAllCollection(nsHTMLDocument* aDocument)
  : mDocument(aDocument)
{
  MOZ_ASSERT(mDocument);
  mozilla::HoldJSObjects(this);
}

HTMLAllCollection::~HTMLAllCollection()
{
  mObject = nullptr;
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(HTMLAllCollection, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(HTMLAllCollection, Release)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLAllCollection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLAllCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLAllCollection)
  tmp->mObject = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HTMLAllCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject*
HTMLAllCollection::GetObject(JSContext* aCx, ErrorResult& aRv)
{
  MOZ_ASSERT(aCx);

  if (!mObject) {
    JS::Rooted<JSObject*> wrapper(aCx, mDocument->GetWrapper());
    MOZ_ASSERT(wrapper);

    JSAutoCompartment ac(aCx, wrapper);
    JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, wrapper));
    mObject = JS_NewObject(aCx, &sHTMLDocumentAllClass, JS::NullPtr(), global);
    if (!mObject) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    // Make the JSObject hold a reference to the document.
    JS_SetPrivate(mObject, ToSupports(mDocument));
    NS_ADDREF(mDocument);
  }

  JS::ExposeObjectToActiveJS(mObject);
  return mObject;
}

} // namespace dom
} // namespace mozilla
