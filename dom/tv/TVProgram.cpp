/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVProgramBinding.h"
#include "TVProgram.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TVProgram, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVProgram)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVProgram)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVProgram)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TVProgram::TVProgram(nsISupports* aOwner)
  : mOwner(aOwner)
{
}

TVProgram::~TVProgram()
{
}

/* virtual */ JSObject*
TVProgram::WrapObject(JSContext* aCx)
{
  return TVProgramBinding::Wrap(aCx, this);
}

void
TVProgram::GetAudioLanguages(nsTArray<nsString>& aLanguages) const
{
  // TODO Implement in follow-up patches.
}

void
TVProgram::GetSubtitleLanguages(nsTArray<nsString>& aLanguages) const
{
  // TODO Implement in follow-up patches.
}

void
TVProgram::GetEventId(nsAString& aEventId) const
{
  // TODO Implement in follow-up patches.
}

already_AddRefed<TVChannel>
TVProgram::Channel() const
{
  // TODO Implement in follow-up patches.
  return nullptr;
}

void
TVProgram::GetTitle(nsAString& aTitle) const
{
  // TODO Implement in follow-up patches.
}

uint64_t
TVProgram::StartTime() const
{
  // TODO Implement in follow-up patches.
  return 0;
}

uint64_t
TVProgram::Duration() const
{
  // TODO Implement in follow-up patches.
  return 0;
}

void
TVProgram::GetDescription(nsAString& aDescription) const
{
  // TODO Implement in follow-up patches.
}

void
TVProgram::GetRating(nsAString& aRating) const
{
  // TODO Implement in follow-up patches.
}

} // namespace dom
} // namespace mozilla
