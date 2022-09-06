/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextClause_h
#define mozilla_dom_TextClause_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class TextClause final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TextClause)

  nsPIDOMWindowInner* GetParentObject() const { return mOwner; }

  TextClause(nsPIDOMWindowInner* aWindow, const TextRange& aRange,
             const TextRange* targetRange);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  inline uint32_t StartOffset() const { return mStartOffset; }

  inline uint32_t EndOffset() const { return mEndOffset; }

  inline bool IsCaret() const { return mIsCaret; }

  inline bool IsTargetClause() const { return mIsTargetClause; }

 private:
  ~TextClause();
  nsCOMPtr<nsPIDOMWindowInner> mOwner;

  // Following members, please take look at widget/TextRange.h.
  uint32_t mStartOffset;
  uint32_t mEndOffset;
  bool mIsCaret;
  bool mIsTargetClause;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TextClause_h
