/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeyError_h
#define mozilla_dom_MediaKeyError_h

#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Event.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class MediaKeyError final : public Event {
 public:
  MediaKeyError(EventTarget* aOwner, uint32_t aSystemCode);
  ~MediaKeyError();

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint32_t SystemCode() const;

 private:
  uint32_t mSystemCode;
};

}  // namespace dom
}  // namespace mozilla

#endif
