/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

// This is loaded by head.js, so has the same globals, hence we import the
// globals from there.
/* import-globals-from shared-head.js */
/* import-globals-from ../mochitest/common.js */

/* exported EVENT_REORDER, EVENT_SHOW, EVENT_TEXT_INSERTED, EVENT_TEXT_REMOVED,
            EVENT_DOCUMENT_LOAD_COMPLETE, EVENT_HIDE, EVENT_TEXT_CARET_MOVED,
            EVENT_DESCRIPTION_CHANGE, EVENT_NAME_CHANGE, EVENT_STATE_CHANGE,
            EVENT_VALUE_CHANGE, EVENT_TEXT_VALUE_CHANGE, EVENT_FOCUS,
            EVENT_DOCUMENT_RELOAD,
            waitForEvent, waitForEvents, waitForOrderedEvents */

const EVENT_DOCUMENT_LOAD_COMPLETE = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE;
const EVENT_HIDE = nsIAccessibleEvent.EVENT_HIDE;
const EVENT_REORDER = nsIAccessibleEvent.EVENT_REORDER;
const EVENT_SHOW = nsIAccessibleEvent.EVENT_SHOW;
const EVENT_STATE_CHANGE = nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_TEXT_CARET_MOVED = nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = nsIAccessibleEvent.EVENT_TEXT_REMOVED;
const EVENT_DESCRIPTION_CHANGE = nsIAccessibleEvent.EVENT_DESCRIPTION_CHANGE;
const EVENT_NAME_CHANGE = nsIAccessibleEvent.EVENT_NAME_CHANGE;
const EVENT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_VALUE_CHANGE;
const EVENT_TEXT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_TEXT_VALUE_CHANGE;
const EVENT_FOCUS = nsIAccessibleEvent.EVENT_FOCUS;
const EVENT_DOCUMENT_RELOAD = nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD;

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

function matchEvent(event, matchCriteria) {
  let acc = event.accessible;
  switch (typeof matchCriteria) {
    case "string":
      let id = getAccessibleDOMNodeID(acc);
      if (id === matchCriteria) {
        Logger.log(`Event matches DOMNode id: ${id}`);
        return true;
      }
      break;
    case "function":
      if (matchCriteria(event)) {
        Logger.log(`Lambda function matches event: ${eventToString(event)}`);
        return true;
      }
      break;
    default:
      if (acc === matchCriteria) {
        Logger.log(`Event matches accessible: ${prettyName(acc)}`);
        return true;
      }
  }

  return false;
}

/**
 * A helper function that returns a promise that resolves when an accessible
 * event of the given type with the given target (defined by its id or
 * accessible) is observed.
 * @param  {String|nsIAccessible|Function}  matchCriteria  expected content
 *                                                         element id
 *                                                         for the event
 * @param  {Number}                eventType        expected accessible event
 *                                                  type
 * @return {Promise}                                promise that resolves to an
 *                                                  event
 */
function waitForEvent(eventType, matchCriteria) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        if (topic !== 'accessible-event') {
          return;
        }

        let event = subject.QueryInterface(nsIAccessibleEvent);
        if (Logger.enabled) {
          // Avoid calling eventToString if the logger isn't enabled in order
          // to avoid an intermittent crash (bug 1307645).
          Logger.log(eventToString(event));
        }

        // If event type does not match expected type, skip the event.
        if (event.eventType !== eventType) {
          return;
        }

        if (matchEvent(event, matchCriteria)) {
          Logger.log(`Correct event type: ${eventTypeToString(eventType)}`);
          ok(event.accessibleDocument instanceof nsIAccessibleDocument,
            'Accessible document present.');

          Services.obs.removeObserver(this, 'accessible-event');
          resolve(event);
        }
      }
    };
    Services.obs.addObserver(eventObserver, 'accessible-event');
  });
}

class UnexpectedEvents {
  constructor(unexpected) {
    if (unexpected.length) {
      this.unexpected = unexpected;
      Services.obs.addObserver(this, 'accessible-event');
    }
  }

  observe(subject, topic, data) {
    if (topic !== 'accessible-event') {
      return;
    }

    let event = subject.QueryInterface(nsIAccessibleEvent);

    let unexpectedEvent = this.unexpected.find(([etype, criteria]) =>
      etype === event.eventType && matchEvent(event, criteria));

    if (unexpectedEvent) {
      ok(false, `Got unexpected event: ${eventToString(event)}`);
    }
  }

  stop() {
    if (this.unexpected) {
      Services.obs.removeObserver(this, 'accessible-event');
    }
  }
}

/**
 * A helper function that waits for a sequence of accessible events in
 * specified order.
 * @param {Array} events        a list of events to wait (same format as
 *                              waitForEvent arguments)
 */
function waitForEvents(events, ordered = false) {
  let expected = events.expected || events;
  let unexpected = events.unexpected || [];
  // Next expected event index.
  let currentIdx = 0;

  let unexpectedListener = new UnexpectedEvents(unexpected);

  return Promise.all(expected.map((evt, idx) => {
    let promise = evt instanceof Array ? waitForEvent(...evt) : evt;
    return promise.then(result => {
      if (ordered) {
        is(idx, currentIdx++,
          `Unexpected event order: ${result}`);
      }
      return result;
    });
  })).then(results => {
    unexpectedListener.stop();
    return results;
  });
}

function waitForOrderedEvents(events) {
  return waitForEvents(events, true);
}
