/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabMessageTypes_h
#define mozilla_dom_TabMessageTypes_h

#include "mozilla/RefPtr.h"

namespace mozilla::dom {
class Event;

struct RemoteDOMEvent {
  // Make sure to set the owner after deserializing.
  RefPtr<Event> mEvent;
};

enum class EmbedderElementEventType {
  NoEvent,
  LoadEvent,
  ErrorEvent,
  EndGuard_,
};

}  // namespace mozilla::dom

#endif  // TABMESSAGE_TYPES_H
