/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

var UrlbarTestUtils = {
  /**
   * Waits to a search to be complete.
   * @param {object} win The window containing the urlbar
   * @returns {Promise} Resolved when done.
   */
  async promiseSearchComplete(win) {
    return this.promisePopupOpen(win, () => {}).then(
      () => win.gURLBar.lastQueryContextPromise
    );
  },

  /**
   * Starts a search for a given string and waits for the search to be complete.
   * @param {object} options.window The window containing the urlbar
   * @param {string} options.value the search string
   * @param {function} options.waitForFocus The SimpleTest function
   * @param {boolean} [options.fireInputEvent] whether an input event should be
   *        used when starting the query (simulates the user's typing, sets
   *        userTypedValued, etc.)
   * @param {number} [options.selectionStart] The input's selectionStart
   * @param {number} [options.selectionEnd] The input's selectionEnd
   */
  async promiseAutocompleteResultPopup({
    window,
    value,
    waitForFocus,
    fireInputEvent = false,
    selectionStart = -1,
    selectionEnd = -1,
  } = {}) {
    await new Promise(resolve => waitForFocus(resolve, window));
    let lastSearchString = window.gURLBar._lastSearchString;
    window.gURLBar.inputField.focus();
    window.gURLBar.value = value;
    if (selectionStart >= 0 && selectionEnd >= 0) {
      window.gURLBar.selectionEnd = selectionEnd;
      window.gURLBar.selectionStart = selectionStart;
    }
    if (fireInputEvent) {
      // This is necessary to get the urlbar to set gBrowser.userTypedValue.
      this.fireInputEvent(window);
    } else {
      window.gURLBar.setPageProxyState("invalid");
    }
    // An input event will start a new search, with a couple of exceptions, so
    // be careful not to call _startSearch if we fired an input event since that
    // would start two searches.  The first exception is when the new search and
    // old search are the same.  Many tests do consecutive searches with the
    // same string and expect new searches to start, so call _startSearch
    // directly then.
    if (!fireInputEvent || value == lastSearchString) {
      this._startSearch(window.gURLBar, value, selectionStart, selectionEnd);
    }
    return this.promiseSearchComplete(window);
  },

  /**
   * Waits for a result to be added at a certain index. Since we implement lazy
   * results replacement, even if we have a result at an index, it may be
   * related to the previous query, this methods ensures the result is current.
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
   * @returns {HtmlElement|XulElement} the result's element.
   */
  async waitForAutocompleteResultAt(win, index) {
    // TODO Bug 1530338: Quantum Bar doesn't yet implement lazy results replacement.
    await this.promiseSearchComplete(win);
    if (index >= win.gURLBar.view._rows.length) {
      throw new Error("Not enough results");
    }
    return win.gURLBar.view._rows.children[index];
  },

  /**
   * Returns the oneOffSearchButtons object for the urlbar.
   * @param {object} win The window containing the urlbar
   * @returns {object} The oneOffSearchButtons
   */
  getOneOffSearchButtons(win) {
    return win.gURLBar.view.oneOffSearchButtons;
  },

  /**
   * Returns true if the oneOffSearchButtons are visible.
   * @param {object} win The window containing the urlbar
   * @returns {boolean} True if the buttons are visible.
   */
  getOneOffSearchButtonsVisible(win) {
    return this.getOneOffSearchButtons(win).style.display != "none";
  },

  _startSearch(urlbar, text, selectionStart = -1, selectionEnd = -1) {
    urlbar.value = text;
    if (selectionStart >= 0 && selectionEnd >= 0) {
      urlbar.selectionEnd = selectionEnd;
      urlbar.selectionStart = selectionStart;
    }
    urlbar.setPageProxyState("invalid");
    urlbar.startQuery();
  },

  /**
   * Gets an abstracted representation of the result at an index.
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
   * @returns {object} An object with numerous properties describing the result.
   */
  async getDetailsOfResultAt(win, index) {
    let element = await this.waitForAutocompleteResultAt(win, index);
    let details = {};
    let result = element.result;
    let { url, postData } = UrlbarUtils.getUrlFromResult(result);
    details.url = url;
    details.postData = postData;
    details.type = result.type;
    details.heuristic = result.heuristic;
    details.autofill = !!result.autofill;
    details.image = element.getElementsByClassName("urlbarView-favicon")[0].src;
    details.title = result.title;
    details.tags = "tags" in result.payload ? result.payload.tags : [];
    let actions = element.getElementsByClassName("urlbarView-action");
    let urls = element.getElementsByClassName("urlbarView-url");
    let typeIcon = element.querySelector(".urlbarView-type-icon");
    let typeIconStyle = win.getComputedStyle(typeIcon);
    details.displayed = {
      title: element.getElementsByClassName("urlbarView-title")[0].textContent,
      action: actions.length > 0 ? actions[0].textContent : null,
      url: urls.length > 0 ? urls[0].textContent : null,
      typeIcon: typeIconStyle["background-image"],
    };
    details.element = {
      action: element.getElementsByClassName("urlbarView-action")[0],
      row: element,
      separator: element.getElementsByClassName(
        "urlbarView-title-separator"
      )[0],
      title: element.getElementsByClassName("urlbarView-title")[0],
      url: element.getElementsByClassName("urlbarView-url")[0],
    };
    if (details.type == UrlbarUtils.RESULT_TYPE.SEARCH) {
      details.searchParams = {
        engine: result.payload.engine,
        keyword: result.payload.keyword,
        query: result.payload.query,
        suggestion: result.payload.suggestion,
      };
    } else if (details.type == UrlbarUtils.RESULT_TYPE.KEYWORD) {
      details.keyword = result.payload.keyword;
    }
    return details;
  },

  /**
   * Gets the currently selected element.
   * @param {object} win The window containing the urlbar
   * @returns {HtmlElement|XulElement} the selected element.
   */
  getSelectedElement(win) {
    return win.gURLBar.view._selected || null;
  },

  /**
   * Gets the index of the currently selected item.
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected index.
   */
  getSelectedIndex(win) {
    return win.gURLBar.view.selectedIndex;
  },

  /**
   * Selects the item at the index specified.
   * @param {object} win The window containing the urlbar.
   * @param {index} index The index to select.
   */
  setSelectedIndex(win, index) {
    win.gURLBar.view.selectedIndex = index;
  },

  /**
   * Gets the number of results.
   * You must wait for the query to be complete before using this.
   * @param {object} win The window containing the urlbar
   * @returns {number} the number of results.
   */
  getResultCount(win) {
    return win.gURLBar.view._rows.children.length;
  },

  getDropMarker(win) {
    return win.gURLBar.dropmarker;
  },

  /**
   * Ensures at least one search suggestion is present.
   * @param {object} win The window containing the urlbar
   * @returns {boolean} whether at least one search suggestion is present.
   */
  promiseSuggestionsPresent(win) {
    // TODO Bug 1530338: Quantum Bar doesn't yet implement lazy results replacement. When
    // we do that, we'll have to be sure the suggestions we find are relevant
    // for the current query. For now let's just wait for the search to be
    // complete.
    return this.promiseSearchComplete(win).then(context => {
      // Look for search suggestions.
      let hasSearchSuggestion = context.results.some(
        r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH && r.payload.suggestion
      );
      if (!hasSearchSuggestion) {
        throw new Error("Cannot find a search suggestion");
      }
    });
  },

  /**
   * Waits for the given number of connections to an http server.
   * @param {object} httpserver an HTTP Server instance
   * @param {number} count Number of connections to wait for
   * @returns {Promise} resolved when all the expected connections were started.
   */
  promiseSpeculativeConnections(httpserver, count) {
    if (!httpserver) {
      throw new Error("Must provide an http server");
    }
    return BrowserTestUtils.waitForCondition(
      () => httpserver.connectionNumber == count,
      "Waiting for speculative connection setup"
    );
  },

  /**
   * Waits for the popup to be shown.
   * @param {object} win The window containing the urlbar
   * @param {function} openFn Function to be used to open the popup.
   * @returns {Promise} resolved once the popup is closed
   */
  async promisePopupOpen(win, openFn) {
    if (!openFn) {
      throw new Error("openFn should be supplied to promisePopupOpen");
    }
    await openFn();
    if (win.gURLBar.view.isOpen) {
      return;
    }
    await new Promise(resolve => {
      win.gURLBar.controller.addQueryListener({
        onViewOpen() {
          win.gURLBar.controller.removeQueryListener(this);
          resolve();
        },
      });
    });
  },

  /**
   * Waits for the popup to be hidden.
   * @param {object} win The window containing the urlbar
   * @param {function} [closeFn] Function to be used to close the popup, if not
   *        supplied it will default to a closing the popup directly.
   * @returns {Promise} resolved once the popup is closed
   */
  async promisePopupClose(win, closeFn = null) {
    if (closeFn) {
      await closeFn();
    } else {
      win.gURLBar.view.close();
    }
    if (!win.gURLBar.view.isOpen) {
      return;
    }
    await new Promise(resolve => {
      win.gURLBar.controller.addQueryListener({
        onViewClose() {
          win.gURLBar.controller.removeQueryListener(this);
          resolve();
        },
      });
    });
  },

  /**
   * @param {object} win The browser window
   * @returns {boolean} Whether the popup is open
   */
  isPopupOpen(win) {
    return win.gURLBar.view.isOpen;
  },

  /**
   * Returns the userContextId (container id) for the last search.
   * @param {object} win The browser window
   * @returns {Promise} resolved when fetching is complete
   * @resolves {number} a userContextId
   */
  async promiseUserContextId(win) {
    const defaultId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
    let context = await win.gURLBar.lastQueryContextPromise;
    return context.userContextId || defaultId;
  },

  /**
   * Dispatches an input event to the input field.
   * @param {object} win The browser window
   */
  fireInputEvent(win) {
    // Set event.data to the last character in the input, for a couple of
    // reasons: It simulates the user typing, and it's necessary for autofill.
    let event = new InputEvent("input", {
      data: win.gURLBar.value[win.gURLBar.value.length - 1] || null,
    });
    win.gURLBar.inputField.dispatchEvent(event);
  },
};
