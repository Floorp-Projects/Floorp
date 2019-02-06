/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
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
    return BrowserTestUtils.waitForPopupEvent(urlbar.panel, "shown").then(async () => {
      await urlbar.promiseSearchComplete();
      if (typeof restoreAnimations == "function") {
        restoreAnimationsFn();
      }
    });
  },

  /**
   * Starts a search for a given string and waits for the search to be complete.
   * @param {object} win The window containing the urlbar
   * @param {string} inputText the search string
   * @param {function} waitForFocus The Simpletest function
   * @param {boolean} fireInputEvent whether an input event should be used when
   *        starting the query (necessary to set userTypedValued)
   */
  async promiseAutocompleteResultPopup(win, inputText, waitForFocus, fireInputEvent = false) {
    let urlbar = getUrlbarAbstraction(win);
    let restoreAnimationsFn = urlbar.disableAnimations();
    await new Promise(resolve => waitForFocus(resolve, win));
    urlbar.focus();
    urlbar.value = inputText;
    if (fireInputEvent) {
      // This is necessary to get the urlbar to set gBrowser.userTypedValue.
      urlbar.fireInputEvent();
    }
    // In the quantum bar it's enough to fire the input event to start a query,
    // invoking startSearch would do it twice.
    if (!urlbar.quantumbar || !fireInputEvent) {
      urlbar.startSearch(inputText);
    }
    return this.promiseSearchComplete(win, restoreAnimationsFn);
  },

  /**
   * Waits for a result to be added at a certain index. Since we implement lazy
   * results replacement, even if we have a result at an index, it may be
   * related to the previous query, this methods ensures the result is current.
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
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
   * Gets an abstracted rapresentation of the result at an index.
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
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
   * Waits for the popup to be hidden.
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
    this.window.addEventListener("unload", () => {
      this.urlbar = null;
      this.window = null;
    }, {once: true});
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
    let event = this.window.document.createEvent("Events");
    event.initEvent("input", true, true);
    this.urlbar.inputField.dispatchEvent(event);
  }

  set value(val) {
    this.urlbar.value = val;
  }
  get value() {
    return this.urlbar.value;
  }

  get panel() {
    return this.quantumbar ? this.urlbar.panel : this.urlbar.popup;
  }

  get oneOffSearchButtons() {
    return this.quantumbar ? this.urlbar.view.oneOffSearchButtons :
           this.urlbar.popup.oneOffSearchButtons;
  }

  startSearch(text) {
    if (this.quantumbar) {
      this.urlbar.value = text;
      this.urlbar.startQuery();
    } else {
      this.urlbar.controller.startSearch(text);
    }
  }

  promiseSearchComplete() {
    if (this.quantumbar) {
      return this.urlbar.lastQueryContextPromise;
    }
    return BrowserTestUtils.waitForCondition(
      () => this.urlbar.controller.searchStatus >=
              Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
      "waiting urlbar search to complete");
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
        () => this.panel.richlistbox.itemChildren.length > index &&
            this.panel.richlistbox.itemChildren[index].getAttribute("ac-text") == searchString.trim(),
        `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`);
      // Ensure the addition is complete, for proper mouse events on the entries.
      await new Promise(resolve => this.window.requestIdleCallback(resolve, {timeout: 1000}));
      return this.panel.richlistbox.itemChildren[index];
    }
    // TODO: Quantum Bar doesn't yet implement lazy results replacement.
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
    return this.panel.selectedIndex >= 0 ?
      this.panel.richlistbox.itemChildren[this.panel.selectedIndex] : null;
  }

  getSelectedIndex() {
    if (!this.quantumbar) {
      return this.panel.selectedIndex;
    }

    return parseInt(this.urlbar.view._selected.getAttribute("resultIndex"));
  }

  getResultCount() {
    return this.quantumbar ? this.urlbar.view._rows.children.length
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
      let context = await this.urlbar.lastQueryContextPromise;
      details.url = (UrlbarUtils.getUrlFromResult(context.results[index])).url;
      details.type = context.results[index].type;
      details.autofill = index == 0 && context.autofillValue;
      details.image = element.getElementsByClassName("urlbarView-favicon")[0].src;
      details.title = context.results[index].title;
      let actions = element.getElementsByClassName("urlbarView-action");
      details.displayed = {
        title: element.getElementsByClassName("urlbarView-title")[0].textContent,
        action: actions.length > 0 ? actions[0].textContent : null,
      };
      if (details.type == UrlbarUtils.RESULT_TYPE.SEARCH) {
        details.searchParams = {
          engine: context.results[index].payload.engine,
          keyword: context.results[index].payload.keyword,
          query: context.results[index].payload.query,
          suggestion: context.results[index].payload.suggestion,
        };
      }
    } else {
      details.url = this.urlbar.controller.getFinalCompleteValueAt(index);
      let style = this.urlbar.controller.getStyleAt(index);
      let action = PlacesUtils.parseActionUrl(this.urlbar.controller.getValueAt(index));
      details.type = getType(style, action);
      details.autofill = style.includes("autofill");
      details.image = element.getAttribute("image");
      details.title = element.getAttribute("title");
      details.displayed = {
        title: element._titleText.textContent,
        action: element._actionText.textContent,
      };
      if (details.type == UrlbarUtils.RESULT_TYPE.SEARCH) {
        details.searchParams = {
          engine: action.params.engineName,
          keyword: action.params.alias,
          query: action.params.input,
          suggestion: action.params.searchSuggestion,
        };
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
          if (action && action.type == "searchengine" &&
              action.params.searchSuggestion) {
            return true;
          }
        }
        return false;
      }, "Waiting for suggestions");
    }
    // TODO: Quantum Bar doesn't yet implement lazy results replacement. When
    // we do that, we'll have to be sure the suggestions we find are relevant
    // for the current query. For now let's just wait for the search to be
    // complete.
    return this.promiseSearchComplete().then(context => {
      // Look for search suggestions.
      if (!context.results.some(r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
                                r.payload.suggestion)) {
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
    if (!this.quantumbar) {
      return BrowserTestUtils.waitForPopupEvent(this.urlbar.popup, "hidden");
    }
    return BrowserTestUtils.waitForPopupEvent(this.urlbar.view.panel, "hidden");
  }
}
