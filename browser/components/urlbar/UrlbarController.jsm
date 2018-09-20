/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["QueryContext", "UrlbarController"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

// XXX This is a fake manager to provide a basic integration test whilst we
// are still constructing the manager.
// eslint-disable-next-line require-jsdoc
const ProvidersManager = {
  queryStart(queryContext, controller) {
    queryContext.results = [];
    for (let i = 0; i < 12; i++) {
      const SWITCH_TO_TAB = Math.random() < .3;
      let url = "http://www." + queryContext.searchString;
      while (Math.random() < .9) {
        url += queryContext.searchString;
      }
      let title = queryContext.searchString;
      while (Math.random() < .5) {
        title += queryContext.isPrivate ? " private" : " foo bar";
      }
      queryContext.results.push({
        title,
        type: SWITCH_TO_TAB ? "switchtotab" : "normal",
        url,
      });
    }
    controller.receiveResults(queryContext);
  },
};

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
 */
class UrlbarController {
  /**
   * Initialises the class. The manager may be overridden here, this is for
   * test purposes.
   *
   * @param {object} [options]
   *   The initial options for UrlbarController.
   * @param {object} [options.manager]
   *   Optional fake providers manager to override the built-in providers manager.
   *   Intended for use in unit tests only.
   */
  constructor(options = {}) {
    this.manager = options.manager || ProvidersManager;

    this._listeners = new Set();
  }

  /**
   * Takes a query context and starts the query based on the user input.
   *
   * @param {QueryContext} queryContext The query details.
   */
  handleQuery(queryContext) {
    queryContext.autoFill = Services.prefs.getBoolPref("browser.urlbar.autoFill", true);

    this._notify("onQueryStarted", queryContext);

    this.manager.queryStart(queryContext, this);
  }

  /**
   * Cancels an in-progress query.
   *
   * @param {QueryContext} queryContext The query details.
   */
  cancelQuery(queryContext) {
    this.manager.queryCancel(queryContext);

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
