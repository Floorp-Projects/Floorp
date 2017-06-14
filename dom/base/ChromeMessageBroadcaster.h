/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeMessageBroadcaster_h
#define mozilla_dom_ChromeMessageBroadcaster_h

#include "mozilla/dom/MessageListenerManager.h"

namespace mozilla {
namespace dom {

class ChromeMessageBroadcaster final : public MessageListenerManager
{
public:
  explicit ChromeMessageBroadcaster(MessageManagerFlags aFlags)
    : ChromeMessageBroadcaster(nullptr, aFlags)
  {
    MOZ_ASSERT(!(aFlags & ~(MessageManagerFlags::MM_GLOBAL |
                            MessageManagerFlags::MM_PROCESSMANAGER |
                            MessageManagerFlags::MM_OWNSCALLBACK)));
  }
  explicit ChromeMessageBroadcaster(nsFrameMessageManager* aParentManager)
    : ChromeMessageBroadcaster(aParentManager, MessageManagerFlags::MM_NONE)
  {}

private:
  ChromeMessageBroadcaster(nsFrameMessageManager* aParentManager,
                           MessageManagerFlags aFlags);
  virtual ~ChromeMessageBroadcaster();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChromeMessageBroadcaster_h
