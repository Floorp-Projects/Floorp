/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeyError.h"
#include "mozilla/dom/MediaKeyErrorBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

MediaKeyError::MediaKeyError(EventTarget* aOwner, uint32_t aSystemCode)
  : Event(aOwner, nullptr, nullptr)
  , mSystemCode(aSystemCode)
{
  InitEvent(NS_LITERAL_STRING("error"), CanBubble::eNo, Cancelable::eNo);
}

MediaKeyError::~MediaKeyError()
{
}

uint32_t
MediaKeyError::SystemCode() const
{
  return mSystemCode;
}

JSObject*
MediaKeyError::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaKeyError_Binding::Wrap(aCx, this, aGivenProto);
}


} // namespace dom
} // namespace mozilla
