/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  FormHistoryTestUtils: "resource://testing-common/FormHistoryTestUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

var UrlbarTestUtils = {
  /**
   * This maps the categories used by the FX_URLBAR_SELECTED_RESULT_METHOD and
   * FX_SEARCHBAR_SELECTED_RESULT_METHOD histograms to their indexes in the
   * `labels` array.  This only needs to be used by tests that need to map from
   * category names to indexes in histogram snapshots.  Actual app code can use
   * these category names directly when they add to a histogram.
   */
  SELECTED_RESULT_METHODS: {
    enter: 0,
    enterSelection: 1,
    click: 2,
    arrowEnterSelection: 3,
    tabEnterSelection: 4,
    rightClickEnter: 5,
  },

  /**
   * Running this init allows helpers to access test scope helpers, like Assert
   * and SimpleTest. Note this initialization is not enforced, thus helpers
   * should always check _testScope and provide a fallback path.
   * @param {object} scope The global scope where tests are being run.
   */
  init(scope) {
    this._testScope = scope;
    if (scope) {
      this.Assert = scope.Assert;
      this.EventUtils = scope.EventUtils;
    }
    // If you add other properties to `this`, null them in uninit().
  },

  /**
   * If tests initialize UrlbarTestUtils, they may need to call this function in
   * their cleanup callback, or else their scope will affect subsequent tests.
   * This is usually only required for tests outside browser/components/urlbar.
   */
  uninit() {
    this._testScope = null;
    this.Assert = null;
    this.EventUtils = null;
  },

  /**
   * Waits to a search to be complete.
   * @param {object} win The window containing the urlbar
   * @returns {Promise} Resolved when done.
   */
  async promiseSearchComplete(win) {
    let waitForQuery = () => {
      return this.promisePopupOpen(win, () => {}).then(
        () => win.gURLBar.lastQueryContextPromise
      );
    };
    let context = await waitForQuery();
    if (win.gURLBar.searchMode) {
      // Search mode may start a second query.
      context = await waitForQuery();
    }
    return context;
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
    if (this._testScope) {
      await this._testScope.SimpleTest.promiseFocus(window);
    } else {
      await new Promise(resolve => waitForFocus(resolve, window));
    }
    window.gURLBar.inputField.focus();
    // Using the value setter in some cases may trim and fetch unexpected
    // results, then pick an alternate path.
    if (UrlbarPrefs.get("trimURLs") && value != BrowserUIUtils.trimURL(value)) {
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
    if (index >= win.gURLBar.view._rows.children.length) {
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
    let buttons = this.getOneOffSearchButtons(win);
    return buttons.style.display != "none" && !buttons.container.hidden;
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
    details.isSponsored = result.payload.isSponsored;
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
    if (this._testScope) {
      this._testScope.info("Awaiting for the urlbar panel to open");
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
    if (this._testScope) {
      this._testScope.info("Awaiting for the urlbar panel to close");
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
   * Open the input field context menu and run a task on it.
   * @param {nsIWindow} win the current window
   * @param {function} task a task function to run, gets the contextmenu popup
   *        as argument.
   */
  async withContextMenu(win, task) {
    let textBox = win.gURLBar.querySelector("moz-input-box");
    let cxmenu = textBox.menupopup;
    let openPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
    this.EventUtils.synthesizeMouseAtCenter(
      win.gURLBar.inputField,
      {
        type: "contextmenu",
        button: 2,
      },
      win
    );
    await openPromise;
    try {
      await task(cxmenu);
    } finally {
      // Close the context menu if the task didn't pick anything.
      if (cxmenu.state == "open" || cxmenu.state == "showing") {
        let closePromise = BrowserTestUtils.waitForEvent(cxmenu, "popuphidden");
        cxmenu.hidePopup();
        await closePromise;
      }
    }
  },

  /**
   * @param {object} win The browser window
   * @returns {boolean} Whether the popup is open
   */
  isPopupOpen(win) {
    return win.gURLBar.view.isOpen;
  },

  /**
   * Asserts that the input is in a given search mode, or no search mode.
   *
   * @param {Window} window
   *   The browser window.
   * @param {object} expectedSearchMode
   *   The expected search mode object.
   * @note Can only be used if UrlbarTestUtils has been initialized with init().
   */
  async assertSearchMode(window, expectedSearchMode) {
    this.Assert.equal(
      !!window.gURLBar.searchMode,
      window.gURLBar.hasAttribute("searchmode"),
      "Urlbar should never be in search mode without the corresponding attribute."
    );

    this.Assert.equal(
      !!window.gURLBar.searchMode,
      !!expectedSearchMode,
      "gURLBar.searchMode should exist as expected"
    );

    if (!expectedSearchMode) {
      // Check the input's placeholder.
      const prefName =
        "browser.urlbar.placeholderName" +
        (PrivateBrowsingUtils.isWindowPrivate(window) ? ".private" : "");
      let engineName = Services.prefs.getStringPref(prefName, "");
      this.Assert.deepEqual(
        window.document.l10n.getAttributes(window.gURLBar.inputField),
        engineName
          ? { id: "urlbar-placeholder-with-name", args: { name: engineName } }
          : { id: "urlbar-placeholder", args: null },
        "Expected placeholder l10n when search mode is inactive"
      );
      return;
    }

    // Default to full search mode for less verbose tests.
    expectedSearchMode = { ...expectedSearchMode };
    if (!expectedSearchMode.hasOwnProperty("isPreview")) {
      expectedSearchMode.isPreview = false;
    }

    // expectedSearchMode may come from UrlbarUtils.LOCAL_SEARCH_MODES.  The
    // objects in that array include useful metadata like icon URIs and pref
    // names that are not usually included in actual search mode objects.  For
    // convenience, ignore those properties if they aren't also present in the
    // urlbar's actual search mode object.
    let ignoreProperties = ["icon", "pref", "restrict"];
    for (let prop of ignoreProperties) {
      if (prop in expectedSearchMode && !(prop in window.gURLBar.searchMode)) {
        if (this._testScope) {
          this._testScope.info(
            `Ignoring unimportant property '${prop}' in expected search mode`
          );
        }
        delete expectedSearchMode[prop];
      }
    }

    this.Assert.deepEqual(
      window.gURLBar.searchMode,
      expectedSearchMode,
      "Expected searchMode"
    );

    // Check the textContent and l10n attributes of the indicator and label.
    let expectedTextContent = "";
    let expectedL10n = { id: null, args: null };
    if (expectedSearchMode.engineName) {
      expectedTextContent = expectedSearchMode.engineName;
    } else if (expectedSearchMode.source) {
      let name = UrlbarUtils.getResultSourceName(expectedSearchMode.source);
      this.Assert.ok(name, "Expected result source should have a name");
      expectedL10n = { id: `urlbar-search-mode-${name}`, args: null };
    } else {
      this.Assert.ok(false, "Unexpected searchMode");
    }

    for (let element of [
      window.gURLBar._searchModeIndicatorTitle,
      window.gURLBar._searchModeLabel,
    ]) {
      if (expectedTextContent) {
        this.Assert.equal(
          element.textContent,
          expectedTextContent,
          "Expected textContent"
        );
      }
      this.Assert.deepEqual(
        window.document.l10n.getAttributes(element),
        expectedL10n,
        "Expected l10n"
      );
    }

    // Check the input's placeholder.
    let expectedPlaceholderL10n;
    if (expectedSearchMode.engineName) {
      expectedPlaceholderL10n = {
        id: UrlbarUtils.WEB_ENGINE_NAMES.has(expectedSearchMode.engineName)
          ? "urlbar-placeholder-search-mode-web-2"
          : "urlbar-placeholder-search-mode-other-engine",
        args: { name: expectedSearchMode.engineName },
      };
    } else if (expectedSearchMode.source) {
      let name = UrlbarUtils.getResultSourceName(expectedSearchMode.source);
      expectedPlaceholderL10n = {
        id: `urlbar-placeholder-search-mode-other-${name}`,
        args: null,
      };
    }
    this.Assert.deepEqual(
      window.document.l10n.getAttributes(window.gURLBar.inputField),
      expectedPlaceholderL10n,
      "Expected placeholder l10n when search mode is active"
    );

    // If this is an engine search mode, check that all results are either
    // search results with the same engine or have the same host as the engine.
    // Search mode preview can show other results since it is not supposed to
    // start a query.
    if (
      expectedSearchMode.engineName &&
      !expectedSearchMode.isPreview &&
      this.isPopupOpen(window)
    ) {
      let resultCount = this.getResultCount(window);
      for (let i = 0; i < resultCount; i++) {
        let result = await this.getDetailsOfResultAt(window, i);
        if (result.source == UrlbarUtils.RESULT_SOURCE.SEARCH) {
          this.Assert.equal(
            expectedSearchMode.engineName,
            result.searchParams.engine,
            "Search mode result matches engine name."
          );
        } else {
          let engine = Services.search.getEngineByName(
            expectedSearchMode.engineName
          );
          let engineRootDomain = UrlbarSearchUtils.getRootDomainFromEngine(
            engine
          );
          let resultUrl = new URL(result.url);
          this.Assert.ok(
            resultUrl.hostname.includes(engineRootDomain),
            "Search mode result matches engine host."
          );
        }
      }
    }
  },

  /**
   * Enters search mode by clicking a one-off.  The view must already be open
   * before you call this.
   * @param {object} window
   * @param {object} searchMode
   *   If given, the one-off matching this search mode will be clicked; it
   *   should be a full search mode object as described in
   *   UrlbarInput.setSearchMode.  If not given, the first one-off is clicked.
   * @note Can only be used if UrlbarTestUtils has been initialized with init().
   */
  async enterSearchMode(window, searchMode = null) {
    // Ensure any pending query is complete.
    await this.promiseSearchComplete(window);

    // Ensure the the one-offs are finished rebuilding and visible.
    let oneOffs = this.getOneOffSearchButtons(window);
    await TestUtils.waitForCondition(
      () => !oneOffs._rebuilding,
      "Waiting for one-offs to finish rebuilding"
    );
    this.Assert.equal(
      UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
      true,
      "One-offs are visible"
    );

    let buttons = oneOffs.getSelectableButtons(true);
    if (!searchMode) {
      searchMode = { engineName: buttons[0].engine.name };
      if (UrlbarUtils.WEB_ENGINE_NAMES.has(searchMode.engineName)) {
        searchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
      }
    }

    if (!searchMode.entry) {
      searchMode.entry = "oneoff";
    }

    let oneOff = buttons.find(o =>
      searchMode.engineName
        ? o.engine.name == searchMode.engineName
        : o.source == searchMode.source
    );
    this.Assert.ok(oneOff, "Found one-off button for search mode");
    this.EventUtils.synthesizeMouseAtCenter(oneOff, {}, window);
    await this.promiseSearchComplete(window);
    this.Assert.ok(this.isPopupOpen(window), "Urlbar view is still open.");
    await this.assertSearchMode(window, searchMode);
  },

  /**
   * Exits search mode.
   * @param {object} window
   * @param {boolean} options.backspace
   *   Exits search mode by backspacing at the beginning of the search string.
   * @param {boolean} options.clickClose
   *   Exits search mode by clicking the close button on the search mode
   *   indicator.
   * @param {boolean} [waitForSearch]
   *   Whether the test should wait for a search after exiting search mode.
   *   Defaults to true.
   * @note If neither `backspace` nor `clickClose` is given, we'll default to
   *       backspacing.
   * @note Can only be used if UrlbarTestUtils has been initialized with init().
   */
  async exitSearchMode(
    window,
    { backspace, clickClose, waitForSearch = true } = {}
  ) {
    let urlbar = window.gURLBar;
    // If the Urlbar is not extended, ignore the clickClose parameter. The close
    // button is not clickable in this state. This state might be encountered on
    // Linux, where prefers-reduced-motion is enabled in automation.
    if (!urlbar.hasAttribute("breakout-extend") && clickClose) {
      if (waitForSearch) {
        let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
        urlbar.searchMode = null;
        await searchPromise;
      } else {
        urlbar.searchMode = null;
      }
      return;
    }

    if (!backspace && !clickClose) {
      backspace = true;
    }

    if (backspace) {
      let urlbarValue = urlbar.value;
      urlbar.selectionStart = urlbar.selectionEnd = 0;
      if (waitForSearch) {
        let searchPromise = this.promiseSearchComplete(window);
        this.EventUtils.synthesizeKey("KEY_Backspace", {}, window);
        await searchPromise;
      } else {
        this.EventUtils.synthesizeKey("KEY_Backspace", {}, window);
      }
      this.Assert.equal(
        urlbar.value,
        urlbarValue,
        "Urlbar value hasn't changed."
      );
      this.assertSearchMode(window, null);
    } else if (clickClose) {
      // We need to hover the indicator to make the close button clickable in the
      // test.
      let indicator = urlbar.querySelector("#urlbar-search-mode-indicator");
      this.EventUtils.synthesizeMouseAtCenter(
        indicator,
        { type: "mouseover" },
        window
      );
      let closeButton = urlbar.querySelector(
        "#urlbar-search-mode-indicator-close"
      );
      if (waitForSearch) {
        let searchPromise = this.promiseSearchComplete(window);
        this.EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);
        await searchPromise;
      } else {
        this.EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);
      }
      await this.assertSearchMode(window, null);
    }
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
            onFirstResult() {
              return false;
            },
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

  /**
   * Initializes some external components used by the urlbar.  This is necessary
   * in xpcshell tests but not in browser tests.
   */
  async initXPCShellDependencies() {
    // The FormHistoryStartup component must be initialized since urlbar uses
    // form history.
    Cc["@mozilla.org/satchel/form-history-startup;1"]
      .getService(Ci.nsIObserver)
      .observe(null, "profile-after-change", null);

    // This is necessary because UrlbarMuxerUnifiedComplete.sort calls
    // Services.search.parseSubmissionURL, so we need engines.
    try {
      await AddonTestUtils.promiseStartupManager();
    } catch (error) {
      if (!error.message.includes("already started")) {
        throw error;
      }
    }
  },
};

UrlbarTestUtils.formHistory = {
  /**
   * Adds values to the urlbar's form history.
   *
   * @param {array} values
   *   The form history entries to remove.
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  add(values = [], window = BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return FormHistoryTestUtils.add(fieldname, values);
  },

  /**
   * Removes values from the urlbar's form history.  If you want to remove all
   * history, use clearFormHistory.
   *
   * @param {array} values
   *   The form history entries to remove.
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  remove(values = [], window = BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return FormHistoryTestUtils.remove(fieldname, values);
  },

  /**
   * Removes all values from the urlbar's form history.  If you want to remove
   * individual values, use removeFormHistory.
   *
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  clear(window = BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return FormHistoryTestUtils.clear(fieldname);
  },

  /**
   * Searches the urlbar's form history.
   *
   * @param {object} criteria
   *   Criteria to narrow the search.  See FormHistory.search.
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise}
   *   A promise resolved with an array of found form history entries.
   */
  search(criteria = {}, window = BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return FormHistoryTestUtils.search(fieldname, criteria);
  },

  /**
   * Returns a promise that's resolved on the next form history change.
   *
   * @param {string} change
   *   Null to listen for any change, or one of: add, remove, update
   * @returns {Promise}
   *   Resolved on the next specified form history change.
   */
  promiseChanged(change = null) {
    return TestUtils.topicObserved(
      "satchel-storage-changed",
      (subject, data) => !change || data == "formhistory-" + change
    );
  },

  /**
   * Returns the form history name for the urlbar in a window.
   *
   * @param {object} window
   *   The window.
   * @returns {string}
   *   The form history name of the urlbar in the window.
   */
  getFormHistoryName(window = BrowserWindowTracker.getTopWindow()) {
    return window ? window.gURLBar.formHistoryName : "searchbar-history";
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
    name = "TestProvider" + uuidGenerator.generateUUID(),
    type = UrlbarUtils.PROVIDER_TYPE.PROFILE,
    priority = 0,
    addTimeout = 0,
    onCancel = null,
    onSelection = null,
  } = {}) {
    super();
    this._results = results;
    this._name = name;
    this._type = type;
    this._priority = priority;
    this._addTimeout = addTimeout;
    this._onCancel = onCancel;
    this._onSelection = onSelection;
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

  onSelection(result, element) {
    if (this._onSelection) {
      this._onSelection(result, element);
    }
  }
}

UrlbarTestUtils.TestProvider = TestProvider;
