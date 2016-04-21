/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global nsIAccessibleEvent, nsIAccessibleDocument,
          nsIAccessibleStateChangeEvent, nsIAccessibleTextChangeEvent */

/* exported EVENT_REORDER, EVENT_SHOW, EVENT_TEXT_INSERTED, EVENT_TEXT_REMOVED,
            EVENT_DOCUMENT_LOAD_COMPLETE, EVENT_HIDE, EVENT_TEXT_CARET_MOVED,
            EVENT_STATE_CHANGE, waitForEvent, waitForMultipleEvents */

const EVENT_DOCUMENT_LOAD_COMPLETE = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE;
const EVENT_HIDE = nsIAccessibleEvent.EVENT_HIDE;
const EVENT_REORDER = nsIAccessibleEvent.EVENT_REORDER;
const EVENT_SHOW = nsIAccessibleEvent.EVENT_SHOW;
const EVENT_STATE_CHANGE = nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_TEXT_CARET_MOVED = nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = nsIAccessibleEvent.EVENT_TEXT_REMOVED;

/**
 * Describe an event in string format.
 * @param  {nsIAccessibleEvent}  event  event to strigify
 */
function eventToString(event) {
  let type = eventTypeToString(event.eventType);
  let info = `Event type: ${type}`;

  if (event instanceof nsIAccessibleStateChangeEvent) {
    let stateStr = statesToString(event.isExtraState ? 0 : event.state,
                                  event.isExtraState ? event.state : 0);
    info += `, state: ${stateStr}, is enabled: ${event.isEnabled}`;
  } else if (event instanceof nsIAccessibleTextChangeEvent) {
    let tcType = event.isInserted ? 'inserted' : 'removed';
    info += `, start: ${event.start}, length: ${event.length}, ${tcType} text: ${event.modifiedText}`;
  }

  info += `. Target: ${prettyName(event.accessible)}`;
  return info;
}

/**
 * A helper function that waits for an accessible event of certain type that
 * belongs to a certain DOMNode (defined by its id).
 * @param  {String}  id         expected content element id for the event
 * @param  {Number}  eventType  expected accessible event type
 * @return {Promise}            promise that resolves to an event
 */
function waitForEvent(eventType, id) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        if (topic !== 'accessible-event') {
          return;
        }

        let event = subject.QueryInterface(nsIAccessibleEvent);
        if (Logger.enabled) {
          Logger.log(eventToString(event));
        }

        let domID = getAccessibleDOMNodeID(event.accessible);
        // If event's accessible does not match expected event type or DOMNode
        // id, skip thie event.
        if (domID === id && event.eventType === eventType) {
          if (Logger.enabled) {
            Logger.log(`Correct event DOMNode id: ${id}`);
            Logger.log(`Correct event type: ${eventTypeToString(eventType)}`);
          }
          ok(event.accessibleDocument instanceof nsIAccessibleDocument,
            'Accessible document present.');

          Services.obs.removeObserver(this, "accessible-event");
          resolve(event);
        }
      }
    };
    Services.obs.addObserver(eventObserver, "accessible-event", false);
  });
}

/**
 * A helper function that waits for a sequence of accessible events in
 * specified order.
 * @param {Array} events        a list of events to wait (same format as
 *                              waitForEvent arguments)
 */
function waitForMultipleEvents(events) {
  // Next expected event index.
  let currentIdx = 0;

  return Promise.all(events.map(({ eventType, id }, idx) =>
    // In addition to waiting for an event, attach an order checker for the
    // event.
    waitForEvent(eventType, id).then(resolvedEvent => {
      // Verify that event happens in order and increment expected index.
      is(idx, currentIdx++,
        `Unexpected event order: ${eventToString(resolvedEvent)}`);
      return resolvedEvent;
    })
  ));
}
