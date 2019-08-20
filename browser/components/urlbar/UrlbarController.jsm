/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarController"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
});

const TELEMETRY_1ST_RESULT = "PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS";
const TELEMETRY_6_FIRST_RESULTS = "PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS";
const NOTIFICATIONS = {
  QUERY_STARTED: "onQueryStarted",
  QUERY_RESULTS: "onQueryResults",
  QUERY_RESULT_REMOVED: "onQueryResultRemoved",
  QUERY_CANCELLED: "onQueryCancelled",
  QUERY_FINISHED: "onQueryFinished",
  VIEW_OPEN: "onViewOpen",
  VIEW_CLOSE: "onViewClose",
};

/**
 * The address bar controller handles queries from the address bar, obtains
 * results and returns them to the UI for display.
 *
 * Listeners may be added to listen for the results. They may support the
 * following methods which may be called when a query is run:
 *
 * - onQueryStarted(queryContext)
 * - onQueryResults(queryContext)
 * - onQueryCancelled(queryContext)
 * - onQueryFinished(queryContext)
 * - onQueryResultRemoved(index)
 * - onViewOpen()
 * - onViewClose()
 */
class UrlbarController {
  /**
   * Initialises the class. The manager may be overridden here, this is for
   * test purposes.
   *
   * @param {object} options
   *   The initial options for UrlbarController.
   * @param {object} options.browserWindow
   *   The browser window this controller is operating within.
   * @param {object} [options.manager]
   *   Optional fake providers manager to override the built-in providers manager.
   *   Intended for use in unit tests only.
   */
  constructor(options = {}) {
    if (!options.browserWindow) {
      throw new Error("Missing options: browserWindow");
    }
    if (
      !options.browserWindow.location ||
      options.browserWindow.location.href != AppConstants.BROWSER_CHROME_URL
    ) {
      throw new Error("browserWindow should be an actual browser window.");
    }

    this.manager = options.manager || UrlbarProvidersManager;
    this.browserWindow = options.browserWindow;

    this._listeners = new Set();
    this._userSelectionBehavior = "none";

    this.engagementEvent = new TelemetryEvent(options.eventTelemetryCategory);
  }

  get NOTIFICATIONS() {
    return NOTIFICATIONS;
  }

  /**
   * Hooks up the controller with an input.
   *
   * @param {UrlbarInput} input
   *   The UrlbarInput instance associated with this controller.
   */
  setInput(input) {
    this.input = input;
  }

  /**
   * Hooks up the controller with a view.
   *
   * @param {UrlbarView} view
   *   The UrlbarView instance associated with this controller.
   */
  setView(view) {
    this.view = view;
  }

  /**
   * Takes a query context and starts the query based on the user input.
   *
   * @param {UrlbarQueryContext} queryContext The query details.
   */
  async startQuery(queryContext) {
    // Cancel any running query.
    this.cancelQuery();

    // Wrap the external queryContext, to track a unique object, in case
    // the external consumer reuses the same context multiple times.
    // This also allows to add properties without polluting the context.
    // Note this can't be null-ed or deleted once a query is done, because it's
    // used by handleDeleteEntry and handleKeyNavigation, that can run after
    // a query is cancelled or finished.
    let contextWrapper = (this._lastQueryContextWrapper = { queryContext });

    queryContext.lastResultCount = 0;
    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT, queryContext);
    TelemetryStopwatch.start(TELEMETRY_6_FIRST_RESULTS, queryContext);

    // For proper functionality we must ensure this notification is fired
    // synchronously, as soon as startQuery is invoked, but after any
    // notifications related to the previous query.
    this.notify(NOTIFICATIONS.QUERY_STARTED, queryContext);
    await this.manager.startQuery(queryContext, this);
    // If the query has been cancelled, onQueryFinished was notified already.
    // Note this._lastQueryContextWrapper may have changed in the meanwhile.
    if (
      contextWrapper === this._lastQueryContextWrapper &&
      !contextWrapper.done
    ) {
      contextWrapper.done = true;
      // TODO (Bug 1549936) this is necessary to avoid leaks in PB tests.
      this.manager.cancelQuery(queryContext);
      this.notify(NOTIFICATIONS.QUERY_FINISHED, queryContext);
    }
    return queryContext;
  }

  /**
   * Cancels an in-progress query. Note, queries may continue running if they
   * can't be cancelled.
   */
  cancelQuery() {
    // If the query finished already, don't handle cancel.
    if (!this._lastQueryContextWrapper || this._lastQueryContextWrapper.done) {
      return;
    }

    this._lastQueryContextWrapper.done = true;

    let { queryContext } = this._lastQueryContextWrapper;
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT, queryContext);
    TelemetryStopwatch.cancel(TELEMETRY_6_FIRST_RESULTS, queryContext);
    this.manager.cancelQuery(queryContext);
    this.notify(NOTIFICATIONS.QUERY_CANCELLED, queryContext);
    this.notify(NOTIFICATIONS.QUERY_FINISHED, queryContext);
  }

  /**
   * Receives results from a query.
   *
   * @param {UrlbarQueryContext} queryContext The query details.
   */
  receiveResults(queryContext) {
    if (queryContext.lastResultCount < 1 && queryContext.results.length >= 1) {
      TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT, queryContext);
    }
    if (queryContext.lastResultCount < 6 && queryContext.results.length >= 6) {
      TelemetryStopwatch.finish(TELEMETRY_6_FIRST_RESULTS, queryContext);
    }

    if (queryContext.lastResultCount == 0 && queryContext.results.length) {
      if (queryContext.results[0].autofill) {
        this.input.autofillFirstResult(queryContext.results[0]);
      }
      // The first time we receive results try to connect to the heuristic
      // result.
      this.speculativeConnect(
        queryContext.results[0],
        queryContext,
        "resultsadded"
      );
    }

    this.notify(NOTIFICATIONS.QUERY_RESULTS, queryContext);
    // Update lastResultCount after notifying, so the view can use it.
    queryContext.lastResultCount = queryContext.results.length;
  }

  /**
   * Adds a listener for query actions and results.
   *
   * @param {object} listener The listener to add.
   * @throws {TypeError} Throws if the listener is not an object.
   */
  addQueryListener(listener) {
    if (!listener || typeof listener != "object") {
      throw new TypeError("Expected listener to be an object");
    }
    this._listeners.add(listener);
  }

  /**
   * Removes a query listener.
   *
   * @param {object} listener The listener to add.
   */
  removeQueryListener(listener) {
    this._listeners.delete(listener);
  }

  /**
   * Checks whether a keyboard event that would normally open the view should
   * instead be handled natively by the input field.
   * On certain platforms, the up and down keys can be used to move the caret,
   * in which case we only want to open the view if the caret is at the
   * start or end of the input.
   *
   * @param {KeyboardEvent} event
   *   The DOM KeyboardEvent.
   * @returns {boolean}
   *   Returns true if the event should move the caret instead of opening the
   *   view.
   */
  keyEventMovesCaret(event) {
    if (this.view.isOpen) {
      return false;
    }
    if (AppConstants.platform != "macosx" && AppConstants.platform != "linux") {
      return false;
    }
    let isArrowUp = event.keyCode == KeyEvent.DOM_VK_UP;
    let isArrowDown = event.keyCode == KeyEvent.DOM_VK_DOWN;
    if (!isArrowUp && !isArrowDown) {
      return false;
    }
    let start = this.input.selectionStart;
    let end = this.input.selectionEnd;
    if (
      end != start ||
      (isArrowUp && start > 0) ||
      (isArrowDown && end < this.input.value.length)
    ) {
      return true;
    }
    return false;
  }

  /**
   * Receives keyboard events from the input and handles those that should
   * navigate within the view or pick the currently selected item.
   *
   * @param {KeyboardEvent} event
   *   The DOM KeyboardEvent.
   * @param {boolean} executeAction
   *   Whether the event should actually execute the associated action, or just
   *   be managed (at a preventDefault() level). This is used when the event
   *   will be deferred by the event bufferer, but preventDefault() and friends
   *   should still happen synchronously.
   */
  handleKeyNavigation(event, executeAction = true) {
    const isMac = AppConstants.platform == "macosx";
    // Handle readline/emacs-style navigation bindings on Mac.
    if (
      isMac &&
      this.view.isOpen &&
      event.ctrlKey &&
      (event.key == "n" || event.key == "p")
    ) {
      if (executeAction) {
        this.view.selectBy(1, { reverse: event.key == "p" });
      }
      event.preventDefault();
      return;
    }

    if (this.view.isOpen && executeAction && this._lastQueryContextWrapper) {
      let { queryContext } = this._lastQueryContextWrapper;
      let handled = this.view.oneOffSearchButtons.handleKeyPress(
        event,
        this.view.visibleItemCount,
        this.view.allowEmptySelection,
        queryContext.searchString
      );
      if (handled) {
        return;
      }
    }

    switch (event.keyCode) {
      case KeyEvent.DOM_VK_ESCAPE:
        if (executeAction) {
          if (this.view.isOpen) {
            this.view.close();
          } else {
            this.input.handleRevert();
          }
        }
        event.preventDefault();
        break;
      case KeyEvent.DOM_VK_RETURN:
        if (executeAction) {
          this.input.handleCommand(event);
        }
        event.preventDefault();
        break;
      case KeyEvent.DOM_VK_TAB:
        if (this.view.isOpen && !event.ctrlKey && !event.altKey) {
          if (executeAction) {
            this.userSelectionBehavior = "tab";
            this.view.selectBy(1, { reverse: event.shiftKey });
          }
          event.preventDefault();
        }
        break;
      case KeyEvent.DOM_VK_DOWN:
      case KeyEvent.DOM_VK_UP:
      case KeyEvent.DOM_VK_PAGE_DOWN:
      case KeyEvent.DOM_VK_PAGE_UP:
        if (event.ctrlKey || event.altKey) {
          break;
        }
        if (this.view.isOpen) {
          if (executeAction) {
            this.userSelectionBehavior = "arrow";
            this.view.selectBy(
              event.keyCode == KeyEvent.DOM_VK_PAGE_DOWN ||
                event.keyCode == KeyEvent.DOM_VK_PAGE_UP
                ? UrlbarUtils.PAGE_UP_DOWN_DELTA
                : 1,
              {
                reverse:
                  event.keyCode == KeyEvent.DOM_VK_UP ||
                  event.keyCode == KeyEvent.DOM_VK_PAGE_UP,
              }
            );
          }
        } else {
          if (this.keyEventMovesCaret(event)) {
            break;
          }
          if (executeAction) {
            this.userSelectionBehavior = "arrow";
            this.engagementEvent.start(event);
            this.input.startQuery({ searchString: this.input.value });
          }
        }
        event.preventDefault();
        break;
      case KeyEvent.DOM_VK_LEFT:
      case KeyEvent.DOM_VK_RIGHT:
      case KeyEvent.DOM_VK_HOME:
      case KeyEvent.DOM_VK_END:
        this.view.removeAccessibleFocus();
        break;
      case KeyEvent.DOM_VK_DELETE:
      case KeyEvent.DOM_VK_BACK_SPACE:
        if (!this.view.isOpen) {
          break;
        }
        if (event.shiftKey) {
          if (!executeAction || this._handleDeleteEntry()) {
            event.preventDefault();
          }
        } else if (executeAction) {
          this.userSelectionBehavior = "none";
        }
        break;
    }
  }

  /**
   * Tries to initialize a speculative connection on a result.
   * Speculative connections are only supported for a subset of all the results.
   * @param {UrlbarResult} result Tthe result to speculative connect to.
   * @param {UrlbarQueryContext} context The queryContext
   * @param {string} reason Reason for the speculative connect request.
   * @note speculative connect to:
   *  - Search engine heuristic results
   *  - autofill results
   *  - http/https results
   */
  speculativeConnect(result, context, reason) {
    // Never speculative connect in private contexts.
    if (!this.input || context.isPrivate || context.results.length == 0) {
      return;
    }
    let { url } = UrlbarUtils.getUrlFromResult(result);
    if (!url) {
      return;
    }

    switch (reason) {
      case "resultsadded": {
        // We should connect to an heuristic result, if it exists.
        if (
          (result == context.results[0] && context.preselected) ||
          result.autofill
        ) {
          if (result.type == UrlbarUtils.RESULT_TYPE.SEARCH) {
            // Speculative connect only if search suggestions are enabled.
            if (
              UrlbarPrefs.get("suggest.searches") &&
              UrlbarPrefs.get("browser.search.suggest.enabled")
            ) {
              let engine = Services.search.defaultEngine;
              UrlbarUtils.setupSpeculativeConnection(
                engine,
                this.browserWindow
              );
            }
          } else if (result.autofill) {
            UrlbarUtils.setupSpeculativeConnection(url, this.browserWindow);
          }
        }
        return;
      }
      case "mousedown": {
        // On mousedown, connect only to http/https urls.
        if (url.startsWith("http")) {
          UrlbarUtils.setupSpeculativeConnection(url, this.browserWindow);
        }
        return;
      }
      default: {
        throw new Error("Invalid speculative connection reason");
      }
    }
  }

  /**
   * Stores the selection behavior that the user has used to select a result.
   *
   * @param {"arrow"|"tab"|"none"} behavior
   *   The behavior the user used.
   */
  set userSelectionBehavior(behavior) {
    // Don't change the behavior to arrow if tab has already been recorded,
    // as we want to know that the tab was used first.
    if (behavior == "arrow" && this._userSelectionBehavior == "tab") {
      return;
    }
    this._userSelectionBehavior = behavior;
  }

  /**
   * Records details of the selected result in telemetry. We only record the
   * selection behavior, type and index.
   *
   * @param {Event} event
   *   The event which triggered the result to be selected.
   * @param {UrlbarResult} result
   *   The selected result.
   */
  recordSelectedResult(event, result) {
    let resultIndex = result ? result.uiIndex : -1;
    let selectedResult = -1;
    if (resultIndex >= 0) {
      // Except for the history popup, the urlbar always has a selection.  The
      // first result at index 0 is the "heuristic" result that indicates what
      // will happen when you press the Enter key.  Treat it as no selection.
      selectedResult = resultIndex > 0 || !result.heuristic ? resultIndex : -1;
    }
    BrowserUsageTelemetry.recordUrlbarSelectedResultMethod(
      event,
      selectedResult,
      this._userSelectionBehavior
    );

    if (!result) {
      return;
    }

    let telemetryType;
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        telemetryType = "switchtab";
        break;
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        telemetryType = result.payload.suggestion
          ? "searchsuggestion"
          : "searchengine";
        break;
      case UrlbarUtils.RESULT_TYPE.URL:
        if (result.autofill) {
          telemetryType = "autofill";
        } else if (
          result.source == UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL &&
          result.heuristic
        ) {
          telemetryType = "visiturl";
        } else {
          telemetryType =
            result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS
              ? "bookmark"
              : "history";
        }
        break;
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        telemetryType = "keyword";
        break;
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        telemetryType = "extension";
        break;
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        telemetryType = "remotetab";
        break;
      default:
        Cu.reportError(`Unknown Result Type ${result.type}`);
        return;
    }

    Services.telemetry
      .getHistogramById("FX_URLBAR_SELECTED_RESULT_INDEX")
      .add(resultIndex);
    // You can add values but don't change any of the existing values.
    // Otherwise you'll break our data.
    if (telemetryType in URLBAR_SELECTED_RESULT_TYPES) {
      Services.telemetry
        .getHistogramById("FX_URLBAR_SELECTED_RESULT_TYPE")
        .add(URLBAR_SELECTED_RESULT_TYPES[telemetryType]);
      Services.telemetry
        .getKeyedHistogramById("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE")
        .add(telemetryType, resultIndex);
    } else {
      Cu.reportError(
        "Unknown FX_URLBAR_SELECTED_RESULT_TYPE type: " + telemetryType
      );
    }
  }

  /**
   * Internal function handling deletion of entries. We only support removing
   * of history entries - other result sources will be ignored.
   *
   * @returns {boolean} Returns true if the deletion was acted upon.
   */
  _handleDeleteEntry() {
    if (!this._lastQueryContextWrapper) {
      Cu.reportError("Cannot delete - the latest query is not present");
      return false;
    }

    const selectedResult = this.input.view.selectedResult;
    if (
      !selectedResult ||
      selectedResult.source != UrlbarUtils.RESULT_SOURCE.HISTORY
    ) {
      return false;
    }

    let { queryContext } = this._lastQueryContextWrapper;
    let index = queryContext.results.indexOf(selectedResult);
    if (!index) {
      Cu.reportError("Failed to find the selected result in the results");
      return false;
    }

    queryContext.results.splice(index, 1);
    this.notify(NOTIFICATIONS.QUERY_RESULT_REMOVED, index);

    PlacesUtils.history
      .remove(selectedResult.payload.url)
      .catch(Cu.reportError);
    return true;
  }

  /**
   * Notifies listeners of results.
   *
   * @param {string} name Name of the notification.
   * @param {object} params Parameters to pass with the notification.
   */
  notify(name, ...params) {
    for (let listener of this._listeners) {
      // Can't use "in" because some tests proxify these.
      if (typeof listener[name] != "undefined") {
        try {
          listener[name](...params);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    }
  }
}

/**
 * Tracks and records telemetry events for the given category, if provided,
 * otherwise it's a no-op.
 * It is currently designed around the "urlbar" category, even if it can
 * potentially be extended to other categories.
 * To record an event, invoke start() with a starting event, then either
 * invoke record() with a final event, or discard() to drop the recording.
 * @see Events.yaml
 */
class TelemetryEvent {
  constructor(category) {
    this._category = category;
  }

  /**
   * Start measuring the elapsed time from an input event.
   * After this has been invoked, any subsequent calls to start() are ignored,
   * until either record() or discard() are invoked. Thus, it is safe to keep
   * invoking this on every input event.
   * @param {event} event A DOM input event.
   * @note This should never throw, or it may break the urlbar.
   */
  start(event) {
    // Start is invoked at any input, but we only count the first one.
    // Once an engagement or abandoment happens, we clear the _startEventInfo.
    if (!this._category || this._startEventInfo) {
      return;
    }
    if (!event) {
      Cu.reportError("Must always provide an event");
      return;
    }
    if (!["input", "drop", "mousedown", "keydown"].includes(event.type)) {
      Cu.reportError("Can't start recording from event type: " + event.type);
      return;
    }

    // "typed" is used when the user types something, while "pasted" and
    // "dropped" are used when the text is inserted at once, by a paste or drop
    // operation. "topsites" is a bit special, it is used when the user opens
    // the empty search dropdown, that is supposed to show top sites. That
    // happens by clicking on the urlbar dropmarker, or pressing DOWN with an
    // empty input field. Even if the user later types something, we still
    // report "topsites", with a positive numChars.
    let interactionType = "topsites";
    if (event.type == "input") {
      interactionType = UrlbarUtils.isPasteEvent(event) ? "pasted" : "typed";
    } else if (event.type == "drop") {
      interactionType = "dropped";
    }

    this._startEventInfo = {
      timeStamp: event.timeStamp || Cu.now(),
      interactionType,
    };
  }

  /**
   * Record an engagement telemetry event.
   * When the user picks a result from a search through the mouse or keyboard,
   * an engagement event is recorded. If instead the user abandons a search, by
   * blurring the input field, an abandonment event is recorded.
   * @param {event} [event] A DOM event.
   * @param {object} details An object describing action details.
   * @param {string} details.numChars Number of input characters.
   * @param {string} details.selIndex Index of the selected result, undefined
   *        for "blur".
   * @param {string} details.selType type of the selected element, undefined
   *        for "blur". One of "none", "autofill", "visit", "bookmark",
   *        "history", "keyword", "search", "searchsuggestion", "switchtab",
   *         "remotetab", "extension", "oneoff".
   * @note event can be null, that usually happens for paste&go or drop&go.
   *       If there's no _startEventInfo this is a no-op.
   */
  record(event, details) {
    // This should never throw, or it may break the urlbar.
    try {
      this._internalRecord(event, details);
    } catch (ex) {
      Cu.reportError("Could not record event: " + ex);
    } finally {
      this._startEventInfo = null;
    }
  }

  _internalRecord(event, details) {
    if (!this._category || !this._startEventInfo) {
      return;
    }
    if (
      !event &&
      this._startEventInfo.interactionType != "pasted" &&
      this._startEventInfo.interactionType != "dropped"
    ) {
      // If no event is passed, we must be executing either paste&go or drop&go.
      throw new Error("Event must be defined, unless input was pasted/dropped");
    }
    if (!details) {
      throw new Error("Invalid event details: " + details);
    }

    let endTime = (event && event.timeStamp) || Cu.now();
    let startTime = this._startEventInfo.timeStamp || endTime;
    // Synthesized events in tests may have a bogus timeStamp, causing a
    // subtraction between monotonic and non-monotonic timestamps; that's why
    // abs is necessary here. It should only happen in tests, anyway.
    let elapsed = Math.abs(Math.round(endTime - startTime));

    let action;
    if (!event) {
      action =
        this._startEventInfo.interactionType == "dropped"
          ? "drop_go"
          : "paste_go";
    } else if (event.type == "blur") {
      action = "blur";
    } else {
      action = event instanceof MouseEvent ? "click" : "enter";
    }
    let method = action == "blur" ? "abandonment" : "engagement";
    let value = this._startEventInfo.interactionType;

    // Rather than listening to the pref, just update status when we record an
    // event, if the pref changed from the last time.
    let recordingEnabled = UrlbarPrefs.get("eventTelemetry.enabled");
    if (this._eventRecordingEnabled != recordingEnabled) {
      this._eventRecordingEnabled = recordingEnabled;
      Services.telemetry.setEventRecordingEnabled("urlbar", recordingEnabled);
    }

    let extra = {
      elapsed: elapsed.toString(),
      numChars: details.numChars.toString(),
    };
    if (method == "engagement") {
      extra.selIndex = details.selIndex.toString();
      extra.selType = details.selType;
    }

    // We invoke recordEvent regardless, if recording is disabled this won't
    // report the events remotely, but will count it in the event_counts scalar.
    Services.telemetry.recordEvent(
      this._category,
      method,
      action,
      value,
      extra
    );
  }

  /**
   * Resets the currently tracked input event, that was registered via start(),
   * so it won't be recorded.
   * If there's no tracked input event, this is a no-op.
   */
  discard() {
    this._startEventInfo = null;
  }

  /**
   * Extracts a type from a result, to be used in the telemetry event.
   * @param {UrlbarResult} result The result to analyze.
   * @returns {string} a string type for the telemetry event.
   */
  typeFromResult(result) {
    if (result) {
      switch (result.type) {
        case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
          return "switchtab";
        case UrlbarUtils.RESULT_TYPE.SEARCH:
          return result.payload.suggestion ? "searchsuggestion" : "search";
        case UrlbarUtils.RESULT_TYPE.URL:
          if (result.autofill) {
            return "autofill";
          }
          if (result.heuristic) {
            return "visit";
          }
          return result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS
            ? "bookmark"
            : "history";
        case UrlbarUtils.RESULT_TYPE.KEYWORD:
          return "keyword";
        case UrlbarUtils.RESULT_TYPE.OMNIBOX:
          return "extension";
        case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
          return "remotetab";
      }
    }
    return "none";
  }
}
