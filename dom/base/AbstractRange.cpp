/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/AbstractRangeBinding.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINode.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbstractRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbstractRange)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbstractRange)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(AbstractRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);
  // mStart and mEnd may depend on or be depended on some other members in
  // concrete classes so that they should be unlinked in sub classes.
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStart)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEnd)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AbstractRange)

AbstractRange::AbstractRange(nsINode* aNode)
    : mIsPositioned(false), mIsGenerated(false), mCalledByJS(false) {
  MOZ_ASSERT(aNode, "range isn't in a document!");
  mOwner = aNode->OwnerDoc();
}

nsINode* AbstractRange::GetCommonAncestor() const {
  return mIsPositioned ? nsContentUtils::GetCommonAncestor(mStart.Container(),
                                                           mEnd.Container())
                       : nullptr;
}

JSObject* AbstractRange::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  MOZ_CRASH("Must be overridden");
}

}  // namespace dom
}  // namespace mozilla
