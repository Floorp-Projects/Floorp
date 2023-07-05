/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnonymousContent_h
#define mozilla_dom_AnonymousContent_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class Element;
class Document;
class ShadowRoot;

class AnonymousContent final {
 public:
  // Ref counting and cycle collection
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnonymousContent)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AnonymousContent)

  static already_AddRefed<AnonymousContent> Create(Document&);

  Element* Host() const { return mHost.get(); }
  ShadowRoot* Root() const { return mRoot.get(); }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

 private:
  ~AnonymousContent();

  explicit AnonymousContent(already_AddRefed<Element> aHost,
                            already_AddRefed<ShadowRoot> aRoot);
  RefPtr<Element> mHost;
  RefPtr<ShadowRoot> mRoot;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AnonymousContent_h
