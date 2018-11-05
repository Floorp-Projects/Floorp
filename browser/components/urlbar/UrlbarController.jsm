/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["QueryContext", "UrlbarController"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  // BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
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
   * Takes a query context and starts the query based on the user input.
   *
   * @param {QueryContext} queryContext The query details.
   */
  async startQuery(queryContext) {
    queryContext.autoFill = Services.prefs.getBoolPref("browser.urlbar.autoFill", true);

    this._notify("onQueryStarted", queryContext);

    await this.manager.startQuery(queryContext, this);
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
   * Handles the case where a url or other text has been entered into the
   * urlbar. This will either load the URL, or some text that could be a keyword
   * or a simple value to load via the default search engine.
   *
   * @param {Event} event The event that triggered this.
   * @param {string} text The text that was entered into the urlbar.
   * @param {string} [openWhere] Where we expect the result to be opened.
   * @param {object} [openParams]
   *   The parameters related to how and where the result will be opened.
   *   For possible properties @see {_loadURL}
   */
  handleEnteredText(event, text, openWhere, openParams = {}) {
    let where = openWhere || this._whereToOpen(event);

    openParams.postData = null;
    openParams.allowInheritPrincipal = false;

    // TODO: Work out how we get the user selection behavior, probably via passing
    // it in, since we don't have the old autocomplete controller to work with.
    // BrowserUsageTelemetry.recordUrlbarSelectedResultMethod(
    //   event, this.userSelectionBehavior);

    text = text.trim();

    try {
      new URL(text);
    } catch (ex) {
      // TODO: Figure out why we need lastLocationChange here.
      // let lastLocationChange = browser.lastLocationChange;
      // UrlbarUtils.getShortcutOrURIAndPostData(text).then(data => {
      //   if (where != "current" ||
      //       browser.lastLocationChange == lastLocationChange) {
      //     params.postData = data.postData;
      //     params.allowInheritPrincipal = data.mayInheritPrincipal;
      //     this._loadURL(data.url, browser, where,
      //                   openUILinkParams);
      //   }
      // });
      return;
    }

    this._loadURL(text, where, openParams);
  }

  /**
   * Opens a specific result that has been selected.
   *
   * @param {Event} event The event that triggered this.
   * @param {UrlbarMatch} result The result that was selected.
   * @param {string} [openWhere] Where we expect the result to be opened.
   * @param {object} [openParams]
   *   The parameters related to how and where the result will be opened.
   *   For possible properties @see {_loadURL}
   */
  resultSelected(event, result, openWhere, openParams = {}) {
    // TODO: Work out how we get the user selection behavior, probably via passing
    // it in, since we don't have the old autocomplete controller to work with.
    // BrowserUsageTelemetry.recordUrlbarSelectedResultMethod(
    //   event, this.userSelectionBehavior);

    let where = openWhere || this._whereToOpen(event);
    openParams.postData = null;
    openParams.allowInheritPrincipal = false;
    let url = result.url;

    switch (result.type) {
      case UrlbarUtils.MATCH_TYPE.TAB_SWITCH: {
        // TODO: Implement handleRevert or equivalent on the input.
        // this.input.handleRevert();
        let prevTab = this.browserWindow.gBrowser.selectedTab;
        let loadOpts = {
          adoptIntoActiveWindow: UrlbarPrefs.get("switchTabs.adoptIntoActiveWindow"),
        };

        if (this.browserWindow.switchToTabHavingURI(url, false, loadOpts) &&
            prevTab.isEmpty) {
          this.browserWindow.gBrowser.removeTab(prevTab);
        }
        return;

        // TODO: How to handle meta chars?
        // Once we get here, we got a TAB_SWITCH match but the user
        // bypassed it by pressing shift/meta/ctrl. Those modifiers
        // might otherwise affect where we open - we always want to
        // open in the current tab.
        // where = "current";

      }
    }

    this._loadURL(url, where, openParams);
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

  /**
   * Loads the url in the appropriate place.
   *
   * @param {string} url
   *   The URL to open.
   * @param {string} openUILinkWhere
   *   Where we expect the result to be opened.
   * @param {object} params
   *   The parameters related to how and where the result will be opened.
   *   Further supported paramters are listed in utilityOverlay.js#openUILinkIn.
   * @param {object} params.triggeringPrincipal
   *   The principal that the action was triggered from.
   * @param {nsIInputStream} [params.postData]
   *   The POST data associated with a search submission.
   * @param {boolean} [params.allowInheritPrincipal]
   *   If the principal may be inherited
   */
  _loadURL(url, openUILinkWhere, params) {
    let browser = this.browserWindow.gBrowser.selectedBrowser;

    // TODO: These should probably be set by the input field.
    // this.value = url;
    // browser.userTypedValue = url;
    if (this.browserWindow.gInitialPages.includes(url)) {
      browser.initialPageLoadedFromURLBar = url;
    }
    try {
      UrlbarUtils.addToUrlbarHistory(url);
    } catch (ex) {
      // Things may go wrong when adding url to session history,
      // but don't let that interfere with the loading of the url.
      Cu.reportError(ex);
    }

    params.allowThirdPartyFixup = true;

    if (openUILinkWhere == "current") {
      params.targetBrowser = browser;
      params.indicateErrorPageLoad = true;
      params.allowPinnedTabHostChange = true;
      params.allowPopups = url.startsWith("javascript:");
    } else {
      params.initiatingDoc = this.browserWindow.document;
    }

    // Focus the content area before triggering loads, since if the load
    // occurs in a new tab, we want focus to be restored to the content
    // area when the current tab is re-selected.
    browser.focus();

    if (openUILinkWhere != "current") {
      // TODO: Implement handleRevert or equivalent on the input.
      // this.input.handleRevert();
    }

    try {
      this.browserWindow.openTrustedLinkIn(url, openUILinkWhere, params);
    } catch (ex) {
      // This load can throw an exception in certain cases, which means
      // we'll want to replace the URL with the loaded URL:
      if (ex.result != Cr.NS_ERROR_LOAD_SHOWED_ERRORPAGE) {
        // TODO: Implement handleRevert or equivalent on the input.
        // this.input.handleRevert();
      }
    }

    // TODO This should probably be handed via input.
    // Ensure the start of the URL is visible for usability reasons.
    // this.selectionStart = this.selectionEnd = 0;
  }

  /**
   * Determines where a URL/page should be opened.
   *
   * @param {Event} event the event triggering the opening.
   * @returns {"current" | "tabshifted" | "tab" | "save" | "window"}
   */
  _whereToOpen(event) {
    let isMouseEvent = event instanceof MouseEvent;
    let reuseEmpty = !isMouseEvent;
    let where = undefined;
    if (!isMouseEvent && event && event.altKey) {
      // We support using 'alt' to open in a tab, because ctrl/shift
      // might be used for canonizing URLs:
      where = event.shiftKey ? "tabshifted" : "tab";
    } else if (!isMouseEvent && this._ctrlCanonizesURLs && event && event.ctrlKey) {
      // If we're allowing canonization, and this is a key event with ctrl
      // pressed, open in current tab to allow ctrl-enter to canonize URL.
      where = "current";
    } else {
      where = this.browserWindow.whereToOpenLink(event, false, false);
    }
    if (this.openInTab) {
      if (where == "current") {
        where = "tab";
      } else if (where == "tab") {
        where = "current";
      }
      reuseEmpty = true;
    }
    if (where == "tab" &&
        reuseEmpty &&
        this.browserWindow.gBrowser.selectedTab.isEmpty) {
      where = "current";
    }
    return where;
  }
}
