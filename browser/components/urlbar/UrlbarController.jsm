/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["QueryContext", "UrlbarController"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  // BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
});

/**
 * QueryContext defines a user's autocomplete input from within the Address Bar.
 * It supplements it with details of how the search results should be obtained
 * and what they consist of.
 */
class QueryContext {
  /**
   * Constructs the QueryContext instance.
   *
   * @param {object} options
   *   The initial options for QueryContext.
   * @param {string} options.searchString
   *   The string the user entered in autocomplete. Could be the empty string
   *   in the case of the user opening the popup via the mouse.
   * @param {number} options.lastKey
   *   The last key the user entered (as a key code). Could be null if the search
   *   was started via the mouse.
   * @param {boolean} options.isPrivate
   *   Set to true if this query was started from a private browsing window.
   * @param {number} options.maxResults
   *   The maximum number of results that will be displayed for this query.
   * @param {boolean} [options.autoFill]
   *   Whether or not to include autofill results. Optional, as this is normally
   *   set by the AddressBarController.
   */
  constructor(options = {}) {
    this._checkRequiredOptions(options, [
      "searchString",
      "lastKey",
      "maxResults",
      "isPrivate",
    ]);

    this.autoFill = !!options.autoFill;
  }

  /**
   * Checks the required options, saving them as it goes.
   *
   * @param {object} options The options object to check.
   * @param {array} optionNames The names of the options to check for.
   * @throws {Error} Throws if there is a missing option.
   */
  _checkRequiredOptions(options, optionNames) {
    for (let optionName of optionNames) {
      if (!(optionName in options)) {
        throw new Error(`Missing or empty ${optionName} provided to QueryContext`);
      }
      this[optionName] = options[optionName];
    }
  }
}

/**
 * The address bar controller handles queries from the address bar, obtains
 * results and returns them to the UI for display.
 *
 * Listeners may be added to listen for the results. They must support the
 * following methods which may be called when a query is run:
 *
 * - onQueryStarted(queryContext)
 * - onQueryResults(queryContext)
 * - onQueryCancelled(queryContext)
 * - onQueryFinished(queryContext)
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
    if (!options.browserWindow.location ||
        options.browserWindow.location.href != AppConstants.BROWSER_CHROME_URL) {
      throw new Error("browserWindow should be an actual browser window.");
    }

    this.manager = options.manager || UrlbarProvidersManager;
    this.browserWindow = options.browserWindow;

    this._listeners = new Set();
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
   * @param {QueryContext} queryContext The query details.
   */
  async startQuery(queryContext) {
    queryContext.autoFill = UrlbarPrefs.get("autoFill");

    this._notify("onQueryStarted", queryContext);

    await this.manager.startQuery(queryContext, this);

    this._notify("onQueryFinished", queryContext);
  }

  /**
   * Cancels an in-progress query. Note, queries may continue running if they
   * can't be canceled.
   *
   * @param {QueryContext} queryContext The query details.
   */
  cancelQuery(queryContext) {
    this.manager.cancelQuery(queryContext);

    this._notify("onQueryCancelled", queryContext);
  }

  /**
   * Receives results from a query.
   *
   * @param {QueryContext} queryContext The query details.
   */
  receiveResults(queryContext) {
    this._notify("onQueryResults", queryContext);
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
   * When switching tabs, clear some internal caches to handle cases like
   * backspace, autofill or repeated searches.
   */
  tabContextChanged() {
    // TODO: implementation needed (bug 1496685)
  }

  /**
   * Receives keyboard events from the input and handles those that should
   * navigate within the view or pick the currently selected item.
   *
   * @param {KeyboardEvent} event
   *   The DOM KeyboardEvent.
   */
  handleKeyNavigation(event) {
    // Handle readline/emacs-style navigation bindings on Mac.
    if (AppConstants.platform == "macosx" &&
        this.view.isOpen &&
        event.ctrlKey &&
        (event.key == "n" || event.key == "p")) {
      this.view.selectNextItem({ reverse: event.key == "p" });
      event.preventDefault();
      return;
    }

    switch (event.keyCode) {
      case KeyEvent.DOM_VK_RETURN:
        if (AppConstants.platform == "macosx" &&
            event.metaKey) {
          // Prevent beep on Mac.
          event.preventDefault();
        }
        // TODO: We may have an autoFill entry, so we should use that instead.
        // TODO: We should have an input bufferrer so that we can use search results
        // if appropriate.
        this.input.handleCommand(event);
        return;
      case KeyEvent.DOM_VK_TAB:
        this.view.selectNextItem({ reverse: event.shiftKey });
        event.preventDefault();
        break;
      case KeyEvent.DOM_VK_DOWN:
        if (!event.ctrlKey && !event.altKey) {
          this.view.selectNextItem();
          event.preventDefault();
        }
        break;
      case KeyEvent.DOM_VK_UP:
        if (!event.ctrlKey && !event.altKey) {
          this.view.selectNextItem({ reverse: true });
          event.preventDefault();
        }
        break;
    }
  }

  /**
   * Internal function to notify listeners of results.
   *
   * @param {string} name Name of the notification.
   * @param {object} params Parameters to pass with the notification.
   */
  _notify(name, ...params) {
    for (let listener of this._listeners) {
      try {
        listener[name](...params);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  }
}
