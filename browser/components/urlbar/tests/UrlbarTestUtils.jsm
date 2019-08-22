/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

var UrlbarTestUtils = {
  /**
   * Waits to a search to be complete.
   * @param {object} win The window containing the urlbar
   * @param {function} restoreAnimationsFn optional Function to be used to
   *        restore animations once done.
   * @returns {Promise} Resolved when done.
   */
  promiseSearchComplete(win, restoreAnimationsFn = null) {
    let urlbar = getUrlbarAbstraction(win);
    return BrowserTestUtils.waitForPopupEvent(urlbar.panel, "shown").then(
      async () => {
        await urlbar.promiseSearchComplete();
        if (typeof restoreAnimations == "function") {
          restoreAnimationsFn();
        }
      }
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
    let urlbar = getUrlbarAbstraction(window);
    let restoreAnimationsFn = urlbar.disableAnimations();
    await new Promise(resolve => waitForFocus(resolve, window));
    let lastSearchString = urlbar.lastSearchString;
    urlbar.focus();
    urlbar.value = value;
    if (selectionStart >= 0 && selectionEnd >= 0) {
      urlbar.selectionEnd = selectionEnd;
      urlbar.selectionStart = selectionStart;
    }
    if (fireInputEvent) {
      // This is necessary to get the urlbar to set gBrowser.userTypedValue.
      urlbar.fireInputEvent();
    } else {
      window.gURLBar.setAttribute("pageproxystate", "invalid");
    }
    // An input event will start a new search, with a couple of exceptions, so
    // be careful not to call startSearch if we fired an input event since that
    // would start two searches.  The first exception is when the new search and
    // old search are the same.  Many tests do consecutive searches with the
    // same string and expect new searches to start, so call startSearch
    // directly then.  The second exception is when searching with the legacy
    // urlbar and an empty string.
    if (
      !fireInputEvent ||
      value == lastSearchString ||
      (!urlbar.quantumbar && !value)
    ) {
      urlbar.startSearch(value, selectionStart, selectionEnd);
    }
    return this.promiseSearchComplete(window, restoreAnimationsFn);
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
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.promiseResultAt(index);
  },

  /**
   * Returns the oneOffSearchButtons object for the urlbar.
   * @param {object} win The window containing the urlbar
   * @returns {object} The oneOffSearchButtons
   */
  getOneOffSearchButtons(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.oneOffSearchButtons;
  },

  /**
   * Returns true if the oneOffSearchButtons are visible.
   * @param {object} win The window containing the urlbar
   * @returns {boolean} True if the buttons are visible.
   */
  getOneOffSearchButtonsVisible(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.oneOffSearchButtonsVisible;
  },

  /**
   * Gets an abstracted rapresentation of the result at an index.
   * @see See the implementation of UrlbarAbstraction.getDetailsOfResultAt.
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
   * @returns {object} An object with numerous properties describing the result.
   */
  async getDetailsOfResultAt(win, index) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.getDetailsOfResultAt(index);
  },

  /**
   * Gets the currently selected element.
   * @param {object} win The window containing the urlbar
   * @returns {HtmlElement|XulElement} the selected element.
   */
  getSelectedElement(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.getSelectedElement();
  },

  /**
   * Gets the index of the currently selected item.
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected index.
   */
  getSelectedIndex(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.getSelectedIndex();
  },

  /**
   * Selects the item at the index specified.
   * @param {object} win The window containing the urlbar.
   * @param {index} index The index to select.
   */
  setSelectedIndex(win, index) {
    let urlbar = getUrlbarAbstraction(win);
    urlbar.setSelectedIndex(index);
  },

  /**
   * Gets the number of results.
   * You must wait for the query to be complete before using this.
   * @param {object} win The window containing the urlbar
   * @returns {number} the number of results.
   */
  getResultCount(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.getResultCount();
  },

  /**
   * Returns the results panel object associated with the window.
   * @note generally tests should use getDetailsOfResultAt rather than
   * accessing panel elements directly.
   * @param {object} win The window containing the urlbar
   * @returns {object} the results panel object.
   */
  getPanel(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.panel;
  },

  getDropMarker(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.dropMarker;
  },

  /**
   * Ensures at least one search suggestion is present.
   * @param {object} win The window containing the urlbar
   * @returns {boolean} whether at least one search suggestion is present.
   */
  promiseSuggestionsPresent(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.promiseSearchSuggestions();
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
  promisePopupOpen(win, openFn) {
    if (!openFn) {
      throw new Error("openFn should be supplied to promisePopupOpen");
    }
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.promisePopupOpen(openFn);
  },

  /**
   * Waits for the popup to be hidden.
   * @param {object} win The window containing the urlbar
   * @param {function} [closeFn] Function to be used to close the popup, if not
   *        supplied it will default to a closing the popup directly.
   * @returns {Promise} resolved once the popup is closed
   */
  promisePopupClose(win, closeFn = null) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.promisePopupClose(closeFn);
  },

  /**
   * @param {object} win The browser window
   * @returns {boolean} Whether the popup is open
   */
  isPopupOpen(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.isPopupOpen();
  },

  /**
   * Returns the userContextId (container id) for the last search.
   * @param {object} win The browser window
   * @returns {Promise} resolved when fetching is complete
   * @resolves {number} a userContextId
   */
  promiseUserContextId(win) {
    let urlbar = getUrlbarAbstraction(win);
    return urlbar.promiseUserContextId();
  },

  /**
   * Dispatches an input event to the input field.
   * @param {object} win The browser window
   */
  fireInputEvent(win) {
    getUrlbarAbstraction(win).fireInputEvent();
  },
};

/**
 * Maps windows to urlbar abstractions.
 */
var gUrlbarAbstractions = new WeakMap();

function getUrlbarAbstraction(win) {
  if (!gUrlbarAbstractions.has(win)) {
    gUrlbarAbstractions.set(win, new UrlbarAbstraction(win));
  }
  return gUrlbarAbstractions.get(win);
}

/**
 * Abstracts the urlbar implementation, so it can be used regardless of
 * Quantum Bar being enabled.
 */
class UrlbarAbstraction {
  constructor(win) {
    if (!win) {
      throw new Error("Must provide a browser window");
    }
    this.urlbar = win.gURLBar;
    this.quantumbar = UrlbarPrefs.get("quantumbar");
    this.window = win;
    this.window.addEventListener(
      "unload",
      () => {
        this.urlbar = null;
        this.window = null;
      },
      { once: true }
    );
  }

  /**
   * Disable animations.
   * @returns {function} can be invoked to restore the previous behavior.
   */
  disableAnimations() {
    if (!this.quantumbar) {
      let dontAnimate = !!this.urlbar.popup.getAttribute("dontanimate");
      this.urlbar.popup.setAttribute("dontanimate", "true");
      return () => {
        this.urlbar.popup.setAttribute("dontanimate", dontAnimate);
      };
    }
    return () => {};
  }

  /**
   * Focus the input field.
   */
  focus() {
    this.urlbar.inputField.focus();
  }

  /**
   * Dispatches an input event to the input field.
   */
  fireInputEvent() {
    // Set event.data to the last character in the input, for a couple of
    // reasons: It simulates the user typing, and it's necessary for autofill.
    let event = new InputEvent("input", {
      data: this.urlbar.value[this.urlbar.value.length - 1] || null,
    });
    this.urlbar.inputField.dispatchEvent(event);
  }

  set value(val) {
    this.urlbar.value = val;
  }
  get value() {
    return this.urlbar.value;
  }

  get lastSearchString() {
    return this.quantumbar
      ? this.urlbar._lastSearchString
      : this.urlbar.controller.searchString;
  }

  get panel() {
    return this.quantumbar ? this.urlbar.panel : this.urlbar.popup;
  }

  get dropMarker() {
    return this.window.document.getAnonymousElementByAttribute(
      this.urlbar.textbox,
      "anonid",
      "historydropmarker"
    );
  }

  get oneOffSearchButtons() {
    return this.quantumbar
      ? this.urlbar.view.oneOffSearchButtons
      : this.urlbar.popup.oneOffSearchButtons;
  }

  get oneOffSearchButtonsVisible() {
    if (!this.quantumbar) {
      return (
        this.window.getComputedStyle(this.oneOffSearchButtons.container)
          .display != "none"
      );
    }

    return this.oneOffSearchButtons.style.display != "none";
  }

  startSearch(text, selectionStart = -1, selectionEnd = -1) {
    if (this.quantumbar) {
      this.urlbar.value = text;
      if (selectionStart >= 0 && selectionEnd >= 0) {
        this.urlbar.selectionEnd = selectionEnd;
        this.urlbar.selectionStart = selectionStart;
      }
      this.urlbar.setAttribute("pageproxystate", "invalid");
      this.urlbar.startQuery();
    } else {
      // Force the controller to do consecutive searches for the same string by
      // calling resetInternalState.
      this.urlbar.controller.resetInternalState();
      this.urlbar.controller.startSearch(text);
    }
  }

  promiseSearchComplete() {
    if (this.quantumbar) {
      return this.urlbar.lastQueryContextPromise;
    }
    return BrowserTestUtils.waitForCondition(
      () =>
        this.urlbar.controller.searchStatus >=
        Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
      "waiting urlbar search to complete",
      100,
      50
    );
  }

  async promiseUserContextId() {
    const defaultId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
    if (this.quantumbar) {
      let context = await this.urlbar.lastQueryContextPromise;
      return context.userContextId || defaultId;
    }
    return this.urlbar.userContextId || defaultId;
  }

  async promiseResultAt(index) {
    if (!this.quantumbar) {
      // In the legacy address bar, old results are replaced when new results
      // arrive, thus it's possible for a result to be present but refer to
      // a previous query. This ensures the given result refers to the current
      // query by checking its query string against the string being searched
      // for.
      let searchString = this.urlbar.controller.searchString;
      await BrowserTestUtils.waitForCondition(
        () =>
          this.panel.richlistbox.itemChildren.length > index &&
          this.panel.richlistbox.itemChildren[index].getAttribute("ac-text") ==
            searchString.trim(),
        `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`
      );
      // Ensure the addition is complete, for proper mouse events on the entries.
      await new Promise(resolve =>
        this.window.requestIdleCallback(resolve, { timeout: 1000 })
      );
      return this.panel.richlistbox.itemChildren[index];
    }
    // TODO Bug 1530338: Quantum Bar doesn't yet implement lazy results replacement.
    await this.promiseSearchComplete();
    if (index >= this.urlbar.view._rows.length) {
      throw new Error("Not enough results");
    }
    return this.urlbar.view._rows.children[index];
  }

  getSelectedElement() {
    if (this.quantumbar) {
      return this.urlbar.view._selected || null;
    }
    return this.panel.selectedIndex >= 0
      ? this.panel.richlistbox.itemChildren[this.panel.selectedIndex]
      : null;
  }

  getSelectedIndex() {
    return this.quantumbar
      ? this.urlbar.view.selectedIndex
      : this.panel.selectedIndex;
  }

  setSelectedIndex(index) {
    if (!this.quantumbar) {
      return (this.panel.selectedIndex = index);
    }
    return (this.urlbar.view.selectedIndex = index);
  }

  getResultCount() {
    return this.quantumbar
      ? this.urlbar.view._rows.children.length
      : this.urlbar.controller.matchCount;
  }

  async getDetailsOfResultAt(index) {
    let element = await this.promiseResultAt(index);
    function getType(style, action) {
      if (style.includes("searchengine") || style.includes("suggestions")) {
        return UrlbarUtils.RESULT_TYPE.SEARCH;
      } else if (style.includes("extension")) {
        return UrlbarUtils.RESULT_TYPE.OMNIBOX;
      } else if (action && action.type == "keyword") {
        return UrlbarUtils.RESULT_TYPE.KEYWORD;
      } else if (action && action.type == "remotetab") {
        return UrlbarUtils.RESULT_TYPE.REMOTE_TAB;
      } else if (action && action.type == "switchtab") {
        return UrlbarUtils.RESULT_TYPE.TAB_SWITCH;
      }
      return UrlbarUtils.RESULT_TYPE.URL;
    }
    let details = {};
    if (this.quantumbar) {
      let result = element.result;
      let { url, postData } = UrlbarUtils.getUrlFromResult(result);
      details.url = url;
      details.postData = postData;
      details.type = result.type;
      details.heuristic = result.heuristic;
      details.autofill = !!result.autofill;
      details.image = element.getElementsByClassName(
        "urlbarView-favicon"
      )[0].src;
      details.title = result.title;
      details.tags = "tags" in result.payload ? result.payload.tags : [];
      let actions = element.getElementsByClassName("urlbarView-action");
      let urls = element.getElementsByClassName("urlbarView-url");
      let typeIcon = element.querySelector(".urlbarView-type-icon");
      let typeIconStyle = this.window.getComputedStyle(typeIcon);
      details.displayed = {
        title: element.getElementsByClassName("urlbarView-title")[0]
          .textContent,
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
    } else {
      details.url = this.urlbar.controller.getFinalCompleteValueAt(index);

      let style = this.urlbar.controller.getStyleAt(index);
      let action = PlacesUtils.parseActionUrl(
        this.urlbar.controller.getValueAt(index)
      );
      details.type = getType(style, action);
      details.heuristic = style.includes("heuristic");
      details.autofill = style.includes("autofill");
      details.image = element.getAttribute("image");
      details.title = element.getAttribute("title");
      details.tags = style.includes("tag")
        ? [...element.getElementsByClassName("ac-tags")].map(e => e.textContent)
        : [];
      let typeIconStyle = this.window.getComputedStyle(element._typeIcon);
      details.displayed = {
        title: element._titleText.textContent,
        action: action ? element._actionText.textContent : "",
        url: element._urlText.textContent,
        typeIcon: typeIconStyle.listStyleImage,
      };
      details.element = {
        action: element._actionText,
        row: element,
        separator: element._separator,
        title: element._titleText,
        url: element._urlText,
      };
      if (details.type == UrlbarUtils.RESULT_TYPE.SEARCH && action) {
        // Strip restriction tokens from input.
        let query = action.params.input;
        let restrictTokens = Object.values(UrlbarTokenizer.RESTRICT);
        if (restrictTokens.includes(query[0])) {
          query = query.substring(1).trim();
        }
        details.searchParams = {
          engine: action.params.engineName,
          keyword: action.params.alias,
          query,
          suggestion: action.params.searchSuggestion,
        };
      } else if (details.type == UrlbarUtils.RESULT_TYPE.KEYWORD) {
        details.keyword = action.params.keyword;
      }
    }
    return details;
  }

  async promiseSearchSuggestions() {
    if (!this.quantumbar) {
      return TestUtils.waitForCondition(() => {
        let controller = this.urlbar.controller;
        let matchCount = controller.matchCount;
        for (let i = 0; i < matchCount; i++) {
          let url = controller.getValueAt(i);
          let action = PlacesUtils.parseActionUrl(url);
          if (
            action &&
            action.type == "searchengine" &&
            action.params.searchSuggestion
          ) {
            return true;
          }
        }
        return false;
      }, "Waiting for suggestions");
    }
    // TODO Bug 1530338: Quantum Bar doesn't yet implement lazy results replacement. When
    // we do that, we'll have to be sure the suggestions we find are relevant
    // for the current query. For now let's just wait for the search to be
    // complete.
    return this.promiseSearchComplete().then(context => {
      // Look for search suggestions.
      if (
        !context.results.some(
          r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH && r.payload.suggestion
        )
      ) {
        throw new Error("Cannot find a search suggestion");
      }
    });
  }

  async promisePopupOpen(openFn) {
    await openFn();
    return BrowserTestUtils.waitForPopupEvent(this.panel, "shown");
  }

  closePopup() {
    if (this.quantumbar) {
      this.urlbar.view.close();
    } else {
      this.urlbar.popup.hidePopup();
    }
  }

  async promisePopupClose(closeFn) {
    if (closeFn) {
      await closeFn();
    } else {
      this.closePopup();
    }
    return BrowserTestUtils.waitForPopupEvent(this.panel, "hidden");
  }

  isPopupOpen() {
    return this.panel.state == "open" || this.panel.state == "showing";
  }
}
