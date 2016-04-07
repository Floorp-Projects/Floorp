/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EventQueue_h_
#define mozilla_a11y_EventQueue_h_

#include "AccEvent.h"

namespace mozilla {
namespace a11y {

class DocAccessible;

/**
 * Used to organize and coalesce pending events.
 */
class EventQueue
{
protected:
  explicit EventQueue(DocAccessible* aDocument) : mDocument(aDocument) { }

  /**
   * Put an accessible event into the queue to process it later.
   */
  bool PushEvent(AccEvent* aEvent);

  /**
   * Puts a name change event into the queue, if needed.
   */
  bool PushNameChange(Accessible* aTarget);

  /**
   * Process events from the queue and fires events.
   */
  void ProcessEventQueue();

private:
  EventQueue(const EventQueue&) = delete;
  EventQueue& operator = (const EventQueue&) = delete;

  // Event queue processing
  /**
   * Coalesce redundant events from the queue.
   */
  void CoalesceEvents();

  /**
   * Coalesce events from the same subtree.
   */
  void CoalesceReorderEvents(AccEvent* aTailEvent);

  /**
   * Coalesce two selection change events within the same select control.
   */
  void CoalesceSelChangeEvents(AccSelChangeEvent* aTailEvent,
                               AccSelChangeEvent* aThisEvent,
                               uint32_t aThisIndex);

protected:
  /**
   * The document accessible reference owning this queue.
   */
  DocAccessible* mDocument;

  /**
   * Pending events array. Don't make this an AutoTArray; we use
   * SwapElements() on it.
   */
  nsTArray<RefPtr<AccEvent>> mEvents;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_EventQueue_h_
