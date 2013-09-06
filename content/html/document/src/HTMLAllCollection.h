/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAllCollection_h
#define mozilla_dom_HTMLAllCollection_h

#include "js/RootingAPI.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

class nsHTMLDocument;

namespace mozilla {
class ErrorResult;

namespace dom {

class HTMLAllCollection
{
public:
  HTMLAllCollection(nsHTMLDocument* aDocument);
  ~HTMLAllCollection();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(HTMLAllCollection)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(HTMLAllCollection)

  JSObject* GetObject(JSContext* aCx, ErrorResult& aRv);

private:
  JS::Heap<JSObject*> mObject;
  nsRefPtr<nsHTMLDocument> mDocument;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAllCollection_h
