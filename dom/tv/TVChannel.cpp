/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "TVChannel.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TVChannel)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TVChannel,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TVChannel,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(TVChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TVChannel, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TVChannel)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TVChannel::TVChannel(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

TVChannel::~TVChannel()
{
}

/* virtual */ JSObject*
TVChannel::WrapObject(JSContext* aCx)
{
  return TVChannelBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
TVChannel::GetPrograms(const TVGetProgramsOptions& aOptions, ErrorResult& aRv)
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
TVChannel::GetNetworkId(nsAString& aNetworkId) const
{
  // TODO Implement in follow-up patches.
}

void
TVChannel::GetTransportStreamId(nsAString& aTransportStreamId) const
{
  // TODO Implement in follow-up patches.
}

void
TVChannel::GetServiceId(nsAString& aServiceId) const
{
  // TODO Implement in follow-up patches.
}

already_AddRefed<TVSource>
TVChannel::Source() const
{
  // TODO Implement in follow-up patches.
  return nullptr;
}

TVChannelType
TVChannel::Type() const
{
  // TODO Implement in follow-up patches.
  return TVChannelType::Tv;
}

void
TVChannel::GetName(nsAString& aName) const
{
  // TODO Implement in follow-up patches.
}

void
TVChannel::GetNumber(nsAString& aNumber) const
{
  // TODO Implement in follow-up patches.
}

bool
TVChannel::IsEmergency() const
{
  // TODO Implement in follow-up patches.
  return false;
}

bool
TVChannel::IsFree() const
{
  // TODO Implement in follow-up patches.
  return false;
}

already_AddRefed<Promise>
TVChannel::GetCurrentProgram(ErrorResult& aRv)
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

} // namespace dom
} // namespace mozilla
