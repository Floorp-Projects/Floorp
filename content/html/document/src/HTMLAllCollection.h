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
#include "nsRefPtrHashtable.h"

#include <stdint.h>

class nsContentList;
class nsHTMLDocument;
class nsIContent;
class nsWrapperCache;

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

  uint32_t Length();
  nsIContent* Item(uint32_t aIndex);

  JSObject* GetObject(JSContext* aCx, ErrorResult& aRv);

  nsISupports* GetNamedItem(const nsAString& aID, nsWrapperCache** aCache);

private:
  nsContentList* Collection();

  /**
   * Returns the NodeList for document.all[aID], or null if there isn't one.
   */
  nsContentList* GetDocumentAllList(const nsAString& aID);

  JS::Heap<JSObject*> mObject;
  nsRefPtr<nsHTMLDocument> mDocument;
  nsRefPtr<nsContentList> mCollection;
  nsRefPtrHashtable<nsStringHashKey, nsContentList> mNamedMap;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAllCollection_h
