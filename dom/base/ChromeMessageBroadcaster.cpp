/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "AccessCheck.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla {
namespace dom {

JSObject*
ChromeMessageBroadcaster::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(nsContentUtils::IsSystemCaller(aCx));

  return ChromeMessageBroadcasterBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
