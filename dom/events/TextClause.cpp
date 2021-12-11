/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextClause.h"
#include "mozilla/dom/TextClauseBinding.h"
#include "mozilla/TextEvents.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(TextClause)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TextClause)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextClause)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextClause)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TextClause::TextClause(nsPIDOMWindowInner* aOwner, const TextRange& aRange,
                       const TextRange* aTargetRange)
    : mOwner(aOwner), mIsTargetClause(false) {
  MOZ_ASSERT(aOwner);
  mStartOffset = aRange.mStartOffset;
  mEndOffset = aRange.mEndOffset;
  if (aRange.IsClause()) {
    mIsCaret = false;
    if (aTargetRange && aTargetRange->mStartOffset == mStartOffset) {
      mIsTargetClause = true;
    }
  } else {
    mIsCaret = true;
  }
}

JSObject* TextClause::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return TextClause_Binding::Wrap(aCx, this, aGivenProto);
}

TextClause::~TextClause() = default;

}  // namespace mozilla::dom
