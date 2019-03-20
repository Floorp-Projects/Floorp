/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "UrlbarEventBufferer",
];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  Log: "resource://gre/modules/Log.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.EventBufferer"));

// Maximum time events can be deferred for.
const DEFERRING_TIMEOUT_MS = 200;

// Array of keyCodes to defer.
const DEFERRED_KEY_CODES = new Set([
  KeyboardEvent.DOM_VK_RETURN,
  KeyboardEvent.DOM_VK_DOWN,
  KeyboardEvent.DOM_VK_TAB,
]);

// Status of the current or last query.
const QUERY_STATUS = {
  UKNOWN: 0,
  RUNNING: 1,
  COMPLETE: 2,
};

/**
 * The UrlbarEventBufferer can queue up events and replay them later, to make
 * the urlbar results more predictable.
 *
 * Search results arrive asynchronously, which means that keydown events may
 * arrive before results do, and therefore not have the effect the user intends.
 * That's especially likely to happen with the down arrow and enter keys, due to
 * the one-off search buttons: if the user very quickly pastes something in the
 * input, presses the down arrow key, and then hits enter, they are probably
 * expecting to visit the first result.  But if there are no results, then
 * pressing down and enter will trigger the first one-off button.
 * To prevent that undesirable behavior, certain keys are buffered and deferred
 * until more results arrive, at which time they're replayed.
 */
class UrlbarEventBufferer {
  /**
   * Initialises the class.
   * @param {UrlbarInput} input The urlbar input object.
   */
  constructor(input) {
    this.input = input;
    this.input.inputField.addEventListener("blur", this);

    // A queue of {event, callback} objects representing deferred events.
    // The callback is invoked when it's the right time to handle the event,
    // but it may also never be invoked, if the context changed and the event
    // became obsolete.
    this._eventsQueue = [];
    // If this timer fires, we will unconditionally replay all the deferred
    // events so that, after a certain point, we don't keep blocking the user's
    // actions, when nothing else has caused the events to be replayed.
    // At that point we won't check whether it's safe to replay the events,
    // because otherwise it may look like we ignored the user's actions.
    this._deferringTimeout = null;

    // Tracks the current or last query status.
    this._lastQuery = {
      // The time at which the current or last search was started. This is used
      // to check how much time passed while deferring the user's actions. Must
      // be set using the monotonic Cu.now() helper.
      startDate: null,
      // Status of the query; one of QUERY_STATUS.*
      status: QUERY_STATUS.UKNOWN,
      // The searchString from the query context.
      searchString: "",
      // The currently returned results.
      results: [],
    };

    // Start listening for queries.
    this.input.controller.addQueryListener(this);
  }

  // UrlbarController listener methods.
  onQueryStarted(queryContext) {
    this._lastQuery = {
      startDate: Cu.now(),
      status: QUERY_STATUS.RUNNING,
      searchString: queryContext.searchString,
      results: [],
    };
    if (this._deferringTimeout) {
      clearTimeout(this._deferringTimeout);
      this._deferringTimeout = null;
    }
  }

  onQueryCancelled(queryContext) {
    this._lastQuery.status = QUERY_STATUS.COMPLETE;
  }

  onQueryFinished(queryContext) {
    this._lastQuery.status = QUERY_STATUS.COMPLETE;
  }

  onQueryResults(queryContext) {
    this._lastQuery.results = queryContext.results;
    // Ensure this runs after other results handling code.
    Services.tm.dispatchToMainThread(() => {
      this.replayDeferredEvents(true);
    });
  }

  /**
   * Handles DOM events.
   * @param {Event} event DOM event from the <textbox>.
   */
  handleEvent(event) {
    if (event.type == "blur") {
      logger.debug("Clearing queue on blur");
      // The input field was blurred, pending events don't matter anymore.
      // Clear the timeout and the queue.
      this._eventsQueue.length = 0;
      if (this._deferringTimeout) {
        clearTimeout(this._deferringTimeout);
        this._deferringTimeout = null;
      }
    }
  }

  /**
   * Receives DOM events, eventually queues them up, and calls back when it's
   * the right time to handle the event.
   * @param {Event} event DOM event from the <textbox>.
   * @param {Function} callback to be invoked when it's the right time to handle
   *        the event.
   */
  maybeDeferEvent(event, callback) {
    if (!callback) {
      throw new Error("Must provide a callback");
    }
    if (this.shouldDeferEvent(event)) {
      this.deferEvent(event, callback);
      return;
    }
    // If it has not been deferred, handle the callback immediately.
    callback();
  }

  /**
   * Adds a deferrable event to the deferred event queue.
   * @param {Event} event The event to defer.
   * @param {Function} callback to be invoked when it's the right time to handle
   *        the event.
   */
  deferEvent(event, callback) {
    // TODO Bug 1536822: once one-off buttons are implemented, figure out if the
    // following is true for the quantum bar as well: somehow event.defaultPrevented
    // ends up true for deferred events.  Autocomplete ignores defaultPrevented
    // events, which means it would ignore replayed deferred events if we didn't
    // tell it to bypass defaultPrevented through urlbarDeferred.
    // Check we don't try to defer events more than once.
    if (event.urlbarDeferred) {
      throw new Error(`Event ${event.type}:${event.keyCode} already deferred!`);
    }
    logger.debug(`Deferring ${event.type}:${event.keyCode} event`);
    // Mark the event as deferred.
    event.urlbarDeferred = true;
    // Also store the current search string, as an added safety check. If the
    // string will differ later, the event is stale and should be dropped.
    event.searchString = this._lastQuery.searchString;
    this._eventsQueue.push({event, callback});

    if (!this._deferringTimeout) {
      let elapsed = Cu.now() - this._lastQuery.startDate;
      let remaining = DEFERRING_TIMEOUT_MS - elapsed;
      this._deferringTimeout = setTimeout(() => {
        this.replayDeferredEvents(false);
        this._deferringTimeout = null;
      }, Math.max(0, remaining));
    }
  }

  /**
   * Replays deferred key events.
   * @param {boolean} onlyIfSafe replays only if it's a safe time to do so.
   *        Setting this to false will replay all the queue events, without any
   *        checks, that is something we want to do only if the deferring
   *        timeout elapsed, and we don't want to appear ignoring user's input.
   */
  replayDeferredEvents(onlyIfSafe) {
    if (typeof onlyIfSafe != "boolean") {
      throw new Error("Must provide a boolean argument");
    }
    if (!this._eventsQueue.length) {
      return;
    }

    let {event, callback} = this._eventsQueue[0];
    if (onlyIfSafe && !this.isSafeToPlayDeferredEvent(event)) {
      return;
    }

    // Remove the event from the queue and play it.
    this._eventsQueue.shift();
    // Safety check: handle only if the search string didn't change meanwhile.
    if (event.searchString == this._lastQuery.searchString) {
      callback();
    }
    Services.tm.dispatchToMainThread(() => {
      this.replayDeferredEvents(onlyIfSafe);
    });
  }

  /**
   * Checks whether a given event should be deferred
   * @param {Event} event The event that should maybe be deferred.
   * @returns {boolean} Whether the event should be deferred.
   */
  shouldDeferEvent(event) {
    // If any event has been deferred for this search, then defer all subsequent
    // events so that the user does not experience them out of order.
    // All events will be replayed when _deferringTimeout fires.
    if (this._eventsQueue.length > 0) {
      return true;
    }

    // At this point, no events have been deferred for this search; we must
    // figure out if this event should be deferred.
    let isMacNavigation = AppConstants.platform == "macosx" &&
                          event.ctrlKey &&
                          this.input.view.isOpen &&
                          (event.key === "n" || event.key === "p");
    if (!DEFERRED_KEY_CODES.has(event.keyCode) && !isMacNavigation) {
      return false;
    }

    if (DEFERRED_KEY_CODES.has(event.keyCode) &&
        this.input.controller.keyEventMovesCaret(event)) {
      return false;
    }

    // This is an event that we'd defer, but if enough time has passed since the
    // start of the search, we don't want to block the user's workflow anymore.
    if (this._lastQuery.startDate + DEFERRING_TIMEOUT_MS <= Cu.now()) {
      return false;
    }

    if (event.keyCode == KeyEvent.DOM_VK_TAB && !this.input.view.isOpen) {
      // The view is closed and the user pressed the Tab key.  The focus should
      // move out of the urlbar immediately.
      return false;
    }

    return !this.isSafeToPlayDeferredEvent(event);
  }

  /**
   * Returns true if the given deferred event can be played now without possibly
   * surprising the user.  This depends on the state of the view, the results,
   * and the type of event.
   * Use this method only after determining that the event should be deferred,
   * or after it has been deferred and you want to know if it can be played now.
   * @param {Event} event The event.
   * @returns {boolean} Whether the event can be played.
   */
  isSafeToPlayDeferredEvent(event) {
    let waitingFirstResult = this._lastQuery.status == QUERY_STATUS.RUNNING &&
                             !this._lastQuery.results.length;
    if (event.keyCode == KeyEvent.DOM_VK_RETURN) {
      // Play a deferred Enter if the heuristic result is not selected, or we
      // are not waiting for the first results yet.
      let selectedResult = this.input.view.selectedResult;
      return (selectedResult && !selectedResult.heuristic) ||
             !waitingFirstResult;
    }

    if (waitingFirstResult || !this.input.view.isOpen) {
      // We're still waiting on the first results, or the popup hasn't opened
      // yet, so not safe.
      return false;
    }

    if (this._lastQuery.status == QUERY_STATUS.COMPLETE) {
      // The view can't get any more results, so there's no need to further
      // defer events.
      return true;
    }

    let isMacDownNavigation = AppConstants.platform == "macosx" &&
                              event.ctrlKey &&
                              this.input.view.isOpen &&
                              event.key === "n";
    if (event.keyCode == KeyEvent.DOM_VK_DOWN || isMacDownNavigation) {
      // Don't play the event if the last result is selected so that the user
      // doesn't accidentally arrow down into the one-off buttons when they
      // didn't mean to.
      return !this.lastResultIsSelected;
    }

    return true;
  }

  get lastResultIsSelected() {
    // TODO Bug 1536818: Once one-off buttons are fully implemented, it would be
    // nice to have a better way to check if the next down will focus one-off buttons.
    let results = this._lastQuery.results;
    return results.length &&
           results[results.length - 1] == this.input.view.selectedResult;
  }
}
