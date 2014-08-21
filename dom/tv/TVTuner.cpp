/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "TVTuner.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TVTuner)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TVTuner,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TVTuner,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(TVTuner, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TVTuner, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TVTuner)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TVTuner::TVTuner(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

TVTuner::~TVTuner()
{
}

/* virtual */ JSObject*
TVTuner::WrapObject(JSContext* aCx)
{
  return TVTunerBinding::Wrap(aCx, this);
}

void
TVTuner::GetSupportedSourceTypes(nsTArray<TVSourceType>& aSourceTypes,
                                 ErrorResult& aRv) const
{
  // TODO Implement in follow-up patches.
}

already_AddRefed<Promise>
TVTuner::GetSources(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // TODO Resolve/reject the promise in follow-up patches.

  return promise.forget();
}

already_AddRefed<Promise>
TVTuner::SetCurrentSource(const TVSourceType aSourceType, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // TODO Resolve/reject the promise in follow-up patches.

  return promise.forget();
}

void
TVTuner::GetId(nsAString& aId) const
{
  // TODO Implement in follow-up patches.
}

already_AddRefed<TVSource>
TVTuner::GetCurrentSource() const
{
  // TODO Implement in follow-up patches.
  return nullptr;
}

already_AddRefed<DOMMediaStream>
TVTuner::GetStream() const
{
  // TODO Implement in follow-up patches.
  return nullptr;
}

} // namespace dom
} // namespace mozilla
