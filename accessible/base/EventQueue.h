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
   * Process events from the queue and fires events.
   */
  void ProcessEventQueue();

private:
  EventQueue(const EventQueue&) MOZ_DELETE;
  EventQueue& operator = (const EventQueue&) MOZ_DELETE;

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

  /**
   * Notify the parent process of events being fired by this event queue.
   */
  void SendIPCEvent(AccEvent* aEvent) const;

  /**
   * Coalesce text change events caused by sibling hide events.
   */
  void CoalesceTextChangeEventsFor(AccHideEvent* aTailEvent,
                                   AccHideEvent* aThisEvent);
  void CoalesceTextChangeEventsFor(AccShowEvent* aTailEvent,
                                   AccShowEvent* aThisEvent);

  /**
    * Create text change event caused by hide or show event. When a node is
    * hidden/removed or shown/appended, the text in an ancestor hyper text will
    * lose or get new characters.
    */
   void CreateTextChangeEventFor(AccMutationEvent* aEvent);

protected:

  /**
   * The document accessible reference owning this queue.
   */
  DocAccessible* mDocument;

  /**
   * Pending events array. Don't make this an nsAutoTArray; we use
   * SwapElements() on it.
   */
  nsTArray<nsRefPtr<AccEvent> > mEvents;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_EventQueue_h_
