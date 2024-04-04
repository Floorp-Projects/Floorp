/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FRAGMENTDIRECTIVE_H_
#define DOM_FRAGMENTDIRECTIVE_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {
class Document;

/**
 * @brief The `FragmentDirective` class is the C++ representation of the
 * `Document.fragmentDirective` webidl property.
 */
class FragmentDirective final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FragmentDirective)

 public:
  explicit FragmentDirective(Document* aDocument);

 protected:
  ~FragmentDirective() = default;

 public:
  Document* GetParentObject() const { return mDocument; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  RefPtr<Document> mDocument;
};

}  // namespace mozilla::dom

#endif  // DOM_FRAGMENTDIRECTIVE_H_
