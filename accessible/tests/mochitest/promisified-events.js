/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded by head.js, so has the same globals, hence we import the
// globals from there.
/* import-globals-from common.js */

/* exported EVENT_ANNOUNCEMENT, EVENT_REORDER, EVENT_SCROLLING,
            EVENT_SCROLLING_END, EVENT_SHOW, EVENT_TEXT_INSERTED,
            EVENT_TEXT_REMOVED, EVENT_DOCUMENT_LOAD_COMPLETE, EVENT_HIDE,
            EVENT_TEXT_ATTRIBUTE_CHANGED, EVENT_TEXT_CARET_MOVED, EVENT_SELECTION,
            EVENT_DESCRIPTION_CHANGE, EVENT_NAME_CHANGE, EVENT_STATE_CHANGE,
            EVENT_VALUE_CHANGE, EVENT_TEXT_VALUE_CHANGE, EVENT_FOCUS,
            EVENT_DOCUMENT_RELOAD, EVENT_VIRTUALCURSOR_CHANGED, EVENT_ALERT,
            EVENT_OBJECT_ATTRIBUTE_CHANGED, UnexpectedEvents, waitForEvent,
            waitForEvents, waitForOrderedEvents, waitForStateChange,
            stateChangeEventArgs */

const EVENT_ANNOUNCEMENT = nsIAccessibleEvent.EVENT_ANNOUNCEMENT;
const EVENT_DOCUMENT_LOAD_COMPLETE =
  nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE;
const EVENT_HIDE = nsIAccessibleEvent.EVENT_HIDE;
const EVENT_REORDER = nsIAccessibleEvent.EVENT_REORDER;
const EVENT_SCROLLING = nsIAccessibleEvent.EVENT_SCROLLING;
const EVENT_SCROLLING_START = nsIAccessibleEvent.EVENT_SCROLLING_START;
const EVENT_SCROLLING_END = nsIAccessibleEvent.EVENT_SCROLLING_END;
const EVENT_SELECTION = nsIAccessibleEvent.EVENT_SELECTION;
const EVENT_SELECTION_WITHIN = nsIAccessibleEvent.EVENT_SELECTION_WITHIN;
const EVENT_SHOW = nsIAccessibleEvent.EVENT_SHOW;
const EVENT_STATE_CHANGE = nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_TEXT_ATTRIBUTE_CHANGED =
  nsIAccessibleEvent.EVENT_TEXT_ATTRIBUTE_CHANGED;
const EVENT_TEXT_CARET_MOVED = nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = nsIAccessibleEvent.EVENT_TEXT_REMOVED;
const EVENT_DESCRIPTION_CHANGE = nsIAccessibleEvent.EVENT_DESCRIPTION_CHANGE;
const EVENT_NAME_CHANGE = nsIAccessibleEvent.EVENT_NAME_CHANGE;
const EVENT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_VALUE_CHANGE;
const EVENT_TEXT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_TEXT_VALUE_CHANGE;
const EVENT_FOCUS = nsIAccessibleEvent.EVENT_FOCUS;
const EVENT_DOCUMENT_RELOAD = nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD;
const EVENT_VIRTUALCURSOR_CHANGED =
  nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED;
const EVENT_ALERT = nsIAccessibleEvent.EVENT_ALERT;
const EVENT_TEXT_SELECTION_CHANGED =
  nsIAccessibleEvent.EVENT_TEXT_SELECTION_CHANGED;
const EVENT_LIVE_REGION_ADDED = nsIAccessibleEvent.EVENT_LIVE_REGION_ADDED;
const EVENT_LIVE_REGION_REMOVED = nsIAccessibleEvent.EVENT_LIVE_REGION_REMOVED;
const EVENT_OBJECT_ATTRIBUTE_CHANGED =
  nsIAccessibleEvent.EVENT_OBJECT_ATTRIBUTE_CHANGED;
const EVENT_INNER_REORDER = nsIAccessibleEvent.EVENT_INNER_REORDER;

const EventsLogger = {
  enabled: false,

  log(msg) {
    if (this.enabled) {
      info(msg);
    }
  },
};

/**
 * Describe an event in string format.
 * @param  {nsIAccessibleEvent}  event  event to strigify
 */
function eventToString(event) {
  let type = eventTypeToString(event.eventType);
  let info = `Event type: ${type}`;

  if (event instanceof nsIAccessibleStateChangeEvent) {
    let stateStr = statesToString(
      event.isExtraState ? 0 : event.state,
      event.isExtraState ? event.state : 0
    );
    info += `, state: ${stateStr}, is enabled: ${event.isEnabled}`;
  } else if (event instanceof nsIAccessibleTextChangeEvent) {
    let tcType = event.isInserted ? "inserted" : "removed";
    info += `, start: ${event.start}, length: ${event.length}, ${tcType} text: ${event.modifiedText}`;
  }

  info += `. Target: ${prettyName(event.accessible)}`;
  return info;
}

function matchEvent(event, matchCriteria) {
  if (!matchCriteria) {
    return true;
  }

  let acc = event.accessible;
  switch (typeof matchCriteria) {
    case "string":
      let id = getAccessibleDOMNodeID(acc);
      if (id === matchCriteria) {
        EventsLogger.log(`Event matches DOMNode id: ${id}`);
        return true;
      }
      break;
    case "function":
      if (matchCriteria(event)) {
        EventsLogger.log(
          `Lambda function matches event: ${eventToString(event)}`
        );
        return true;
      }
      break;
    default:
      if (matchCriteria instanceof nsIAccessible) {
        if (acc === matchCriteria) {
          EventsLogger.log(`Event matches accessible: ${prettyName(acc)}`);
          return true;
        }
      } else if (event.DOMNode == matchCriteria) {
        EventsLogger.log(
          `Event matches DOM node: ${prettyName(event.DOMNode)}`
        );
        return true;
      }
  }

  return false;
}

/**
 * A helper function that returns a promise that resolves when an accessible
 * event of the given type with the given target (defined by its id or
 * accessible) is observed.
 * @param  {Number}                eventType        expected accessible event
 *                                                  type
 * @param  {String|nsIAccessible|Function}  matchCriteria  expected content
 *                                                         element id
 *                                                         for the event
 * @param  {String}                message          Message to prepend to logging.
 * @return {Promise}                                promise that resolves to an
 *                                                  event
 */
function waitForEvent(eventType, matchCriteria, message) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        if (topic !== "accessible-event") {
          return;
        }

        let event = subject.QueryInterface(nsIAccessibleEvent);
        if (EventsLogger.enabled) {
          // Avoid calling eventToString if the EventsLogger isn't enabled in order
          // to avoid an intermittent crash (bug 1307645).
          EventsLogger.log(eventToString(event));
        }

        // If event type does not match expected type, skip the event.
        if (event.eventType !== eventType) {
          return;
        }

        if (matchEvent(event, matchCriteria)) {
          EventsLogger.log(
            `Correct event type: ${eventTypeToString(eventType)}`
          );
          Services.obs.removeObserver(this, "accessible-event");
          ok(
            true,
            `${message ? message + ": " : ""}Received ${eventTypeToString(
              eventType
            )} event`
          );
          resolve(event);
        }
      },
    };
    Services.obs.addObserver(eventObserver, "accessible-event");
  });
}

class UnexpectedEvents {
  constructor(unexpected) {
    if (unexpected.length) {
      this.unexpected = unexpected;
      Services.obs.addObserver(this, "accessible-event");
    }
  }

  observe(subject, topic, data) {
    if (topic !== "accessible-event") {
      return;
    }

    let event = subject.QueryInterface(nsIAccessibleEvent);

    let unexpectedEvent = this.unexpected.find(
      ([etype, criteria]) =>
        etype === event.eventType && matchEvent(event, criteria)
    );

    if (unexpectedEvent) {
      ok(false, `Got unexpected event: ${eventToString(event)}`);
    }
  }

  stop() {
    if (this.unexpected) {
      Services.obs.removeObserver(this, "accessible-event");
    }
  }
}

/**
 * A helper function that waits for a sequence of accessible events in
 * specified order.
 * @param {Array}   events          a list of events to wait (same format as
 *                                   waitForEvent arguments)
 * @param {String}  message         Message to prepend to logging.
 * @param {Boolean} ordered         Events need to be received in given order.
 * @param {Object}  invokerOrWindow a local window or a special content invoker
 *                                   it takes a list of arguments and a task
 *                                   function.
 */
async function waitForEvents(
  events,
  message,
  ordered = false,
  invokerOrWindow = null
) {
  let expected = events.expected || events;
  // Next expected event index.
  let currentIdx = 0;

  let unexpectedListener = events.unexpected
    ? new UnexpectedEvents(events.unexpected)
    : null;

  let results = await Promise.all(
    expected.map((evt, idx) => {
      const [eventType, matchCriteria] = evt;
      return waitForEvent(eventType, matchCriteria, message).then(result => {
        return [result, idx == currentIdx++];
      });
    })
  );

  if (unexpectedListener) {
    let flushQueue = async win => {
      // Flush all notifications or queued a11y events.
      win.windowUtils.advanceTimeAndRefresh(100);

      // Flush all DOM async events.
      await new Promise(r => win.setTimeout(r, 0));

      // Flush all notifications or queued a11y events resulting from async DOM events.
      win.windowUtils.advanceTimeAndRefresh(100);

      // Flush all notifications or a11y events that may have been queued in the last tick.
      win.windowUtils.advanceTimeAndRefresh(100);

      // Return refresh to normal.
      win.windowUtils.restoreNormalRefresh();
    };

    if (invokerOrWindow instanceof Function) {
      await invokerOrWindow([flushQueue.toString()], async _flushQueue => {
        // eslint-disable-next-line no-eval, no-undef
        await eval(_flushQueue)(content);
      });
    } else {
      await flushQueue(invokerOrWindow ? invokerOrWindow : window);
    }

    unexpectedListener.stop();
  }

  if (ordered) {
    ok(
      results.every(([, isOrdered]) => isOrdered),
      `${message ? message + ": " : ""}Correct event order`
    );
  }

  return results.map(([event]) => event);
}

function waitForOrderedEvents(events, message) {
  return waitForEvents(events, message, true);
}

function stateChangeEventArgs(id, state, isEnabled, isExtra = false) {
  return [
    EVENT_STATE_CHANGE,
    e => {
      e.QueryInterface(nsIAccessibleStateChangeEvent);
      return (
        e.state == state &&
        e.isExtraState == isExtra &&
        isEnabled == e.isEnabled &&
        (typeof id == "string"
          ? id == getAccessibleDOMNodeID(e.accessible)
          : getAccessible(id) == e.accessible)
      );
    },
  ];
}

function waitForStateChange(id, state, isEnabled, isExtra = false) {
  return waitForEvent(...stateChangeEventArgs(id, state, isEnabled, isExtra));
}

////////////////////////////////////////////////////////////////////////////////
// Utility functions ported from events.js.

/**
 * This function selects all text in the passed-in element if it has an editor,
 * before setting focus to it. This simulates behavio with the keyboard when
 * tabbing to the element. This does explicitly what synthFocus did implicitly.
 * This should be called only if you really want this behavior.
 * @param  {string}  id  The element ID to focus
 */
function selectAllTextAndFocus(id) {
  const elem = getNode(id);
  if (elem.editor) {
    elem.selectionStart = elem.selectionEnd = elem.value.length;
  }

  elem.focus();
}
