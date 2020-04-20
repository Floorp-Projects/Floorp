/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
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
   *        userTypedValued, triggers engagement event telemetry, etc.)
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
    window.gURLBar.inputField.focus();
    // Using the value setter in some cases may trim and fetch unexpected
    // results, then pick an alternate path.
    if (UrlbarPrefs.get("trimURLs") && value != BrowserUtils.trimURL(value)) {
      window.gURLBar.inputField.value = value;
      fireInputEvent = true;
    } else {
      window.gURLBar.value = value;
    }
    if (selectionStart >= 0 && selectionEnd >= 0) {
      window.gURLBar.selectionEnd = selectionEnd;
      window.gURLBar.selectionStart = selectionStart;
    }

    // An input event will start a new search, so be careful not to start a
    // search if we fired an input event since that would start two searches.
    if (fireInputEvent) {
      // This is necessary to get the urlbar to set gBrowser.userTypedValue.
      this.fireInputEvent(window);
    } else {
      window.gURLBar.setPageProxyState("invalid");
      window.gURLBar.startQuery();
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
    details.source = result.source;
    details.heuristic = result.heuristic;
    details.autofill = !!result.autofill;
    details.image = element.getElementsByClassName("urlbarView-favicon")[0].src;
    details.title = result.title;
    details.tags = "tags" in result.payload ? result.payload.tags : [];
    let actions = element.getElementsByClassName("urlbarView-action");
    let urls = element.getElementsByClassName("urlbarView-url");
    let typeIcon = element.querySelector(".urlbarView-type-icon");
    await win.document.l10n.translateFragment(element);
    details.displayed = {
      title: element.getElementsByClassName("urlbarView-title")[0].textContent,
      action: actions.length ? actions[0].textContent : null,
      url: urls.length ? urls[0].textContent : null,
      typeIcon: typeIcon
        ? win.getComputedStyle(typeIcon)["background-image"]
        : null,
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
        inPrivateWindow: result.payload.inPrivateWindow,
        isPrivateEngine: result.payload.isPrivateEngine,
      };
    } else if (details.type == UrlbarUtils.RESULT_TYPE.KEYWORD) {
      details.keyword = result.payload.keyword;
    }
    return details;
  },

  /**
   * Gets the currently selected element.
   * @param {object} win The window containing the urlbar.
   * @returns {HtmlElement|XulElement} The selected element.
   */
  getSelectedElement(win) {
    return win.gURLBar.view.selectedElement || null;
  },

  /**
   * Gets the index of the currently selected element.
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected index.
   */
  getSelectedElementIndex(win) {
    return win.gURLBar.view.selectedElementIndex;
  },

  /**
   * Gets the currently selected row. If the selected element is a descendant of
   * a row, this will return the ancestor row.
   * @param {object} win The window containing the urlbar.
   * @returns {HTMLElement|XulElement} The selected row.
   */
  getSelectedRow(win) {
    return win.gURLBar.view._getSelectedRow() || null;
  },

  /**
   * Gets the index of the currently selected element.
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected row index.
   */
  getSelectedRowIndex(win) {
    return win.gURLBar.view.selectedRowIndex;
  },

  /**
   * Selects the element at the index specified.
   * @param {object} win The window containing the urlbar.
   * @param {index} index The index to select.
   */
  setSelectedRowIndex(win, index) {
    win.gURLBar.view.selectedRowIndex = index;
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
      let firstSearchSuggestionIndex = context.results.findIndex(
        r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH && r.payload.suggestion
      );
      if (firstSearchSuggestionIndex == -1) {
        throw new Error("Cannot find a search suggestion");
      }
      return firstSearchSuggestionIndex;
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

  /**
   * Returns a new mock controller.  This is useful for xpcshell tests.
   * @param {object} options Additional options to pass to the UrlbarController
   *        constructor.
   * @returns {UrlbarController} A new controller.
   */
  newMockController(options = {}) {
    return new UrlbarController(
      Object.assign(
        {
          input: {
            isPrivate: false,
            window: {
              location: {
                href: AppConstants.BROWSER_CHROME_URL,
              },
            },
          },
        },
        options
      )
    );
  },
};

/**
 * A test provider.  If you need a test provider whose behavior is different
 * from this, then consider modifying the implementation below if you think the
 * new behavior would be useful for other tests.  Otherwise, you can create a
 * new TestProvider instance and then override its methods.
 */
class TestProvider extends UrlbarProvider {
  /**
   * Constructor.
   *
   * @param {array} results
   *   An array of UrlbarResult objects that will be the provider's results.
   * @param {string} [name]
   *   The provider's name.  Provider names should be unique.
   * @param {UrlbarUtils.PROVIDER_TYPE} [type]
   *   The provider's type.
   * @param {number} [priority]
   *   The provider's priority.  Built-in providers have a priority of zero.
   * @param {number} [addTimeout]
   *   If non-zero, each result will be added on this timeout.  If zero, all
   *   results will be added immediately and synchronously.
   * @param {function} [onCancel]
   *   If given, a function that will be called when the provider's cancelQuery
   *   method is called.
   */
  constructor({
    results,
    name = "TestProvider" + Math.floor(Math.random() * 100000),
    type = UrlbarUtils.PROVIDER_TYPE.PROFILE,
    priority = 0,
    addTimeout = 0,
    onCancel = null,
  } = {}) {
    super();
    this._results = results;
    this._name = name;
    this._type = type;
    this._priority = priority;
    this._addTimeout = addTimeout;
    this._onCancel = onCancel;
  }
  get name() {
    return this._name;
  }
  get type() {
    return this._type;
  }
  getPriority(context) {
    return this._priority;
  }
  isActive(context) {
    return true;
  }
  async startQuery(context, addCallback) {
    for (let result of this._results) {
      if (!this._addTimeout) {
        addCallback(this, result);
      } else {
        await new Promise(resolve => {
          setTimeout(() => {
            addCallback(this, result);
            resolve();
          }, this._addTimeout);
        });
      }
    }
  }
  cancelQuery(context) {
    if (this._onCancel) {
      this._onCancel();
    }
  }
  pickResult(result) {}
}

UrlbarTestUtils.TestProvider = TestProvider;
