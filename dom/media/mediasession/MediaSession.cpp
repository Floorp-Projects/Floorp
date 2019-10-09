/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSession.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MediaSession)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaSession)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaSession)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaSession)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsIGlobalObject* MediaSession::GetParentObject() const { return nullptr; }

JSObject* MediaSession::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MediaSession_Binding::Wrap(aCx, this, aGivenProto);
}

MediaMetadata* MediaSession::GetMetadata() const { return nullptr; }

void MediaSession::SetMetadata(MediaMetadata* aMetadata) {}

void MediaSession::SetActionHandler(MediaSessionAction aAction,
                                    MediaSessionActionHandler* aHandler) {}

}  // namespace dom
}  // namespace mozilla
