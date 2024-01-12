/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.sys.mjs",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",

  FormHistoryTestUtils:
    "resource://testing-common/FormHistoryTestUtils.sys.mjs",

  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  UrlbarController: "resource:///modules/UrlbarController.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

export var UrlbarTestUtils = {
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

  // Fallback to the console.
  info: console.log,

  /**
   * Running this init allows helpers to access test scope helpers, like Assert
   * and SimpleTest. Note this initialization is not enforced, thus helpers
   * should always check the properties set here and provide a fallback path.
   *
   * @param {object} scope The global scope where tests are being run.
   */
  init(scope) {
    if (!scope) {
      throw new Error("Must initialize UrlbarTestUtils with a test scope");
    }
    // If you add other properties to `this`, null them in uninit().
    this.Assert = scope.Assert;
    this.info = scope.info;
    this.registerCleanupFunction = scope.registerCleanupFunction;

    if (Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      this.initXPCShellDependencies();
    } else {
      // xpcshell doesn't support EventUtils.
      this.EventUtils = scope.EventUtils;
      this.SimpleTest = scope.SimpleTest;
    }

    this.registerCleanupFunction(() => {
      this.Assert = null;
      this.info = console.log;
      this.registerCleanupFunction = null;
      this.EventUtils = null;
      this.SimpleTest = null;
    });
  },

  /**
   * Waits to a search to be complete.
   *
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
    if (win.gURLBar.view.oneOffSearchButtons._rebuilding) {
      await new Promise(resolve =>
        win.gURLBar.view.oneOffSearchButtons.addEventListener(
          "rebuild",
          resolve,
          {
            once: true,
          }
        )
      );
    }
    return context;
  },

  /**
   * Starts a search for a given string and waits for the search to be complete.
   *
   * @param {object} options The options object.
   * @param {object} options.window The window containing the urlbar
   * @param {string} options.value the search string
   * @param {Function} options.waitForFocus The SimpleTest function
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
    fireInputEvent = true,
    selectionStart = -1,
    selectionEnd = -1,
  } = {}) {
    if (this.SimpleTest) {
      await this.SimpleTest.promiseFocus(window);
    } else {
      await new Promise(resolve => waitForFocus(resolve, window));
    }

    const setup = () => {
      window.gURLBar.focus();
      // Using the value setter in some cases may trim and fetch unexpected
      // results, then pick an alternate path.
      if (
        lazy.UrlbarPrefs.get("trimURLs") &&
        value != lazy.BrowserUIUtils.trimURL(value)
      ) {
        window.gURLBar._setValue(value, false);
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
    };
    setup();

    // In Linux TV test, as there is case that the input field lost the focus
    // until showing popup, timeout failure happens since the expected poup
    // never be shown. To avoid this, if losing the focus, retry setup to open
    // popup.
    const blurListener = () => {
      setup();
    };
    window.gURLBar.inputField.addEventListener("blur", blurListener, {
      once: true,
    });
    const result = await this.promiseSearchComplete(window);
    window.gURLBar.inputField.removeEventListener("blur", blurListener);
    return result;
  },

  /**
   * Waits for a result to be added at a certain index. Since we implement lazy
   * results replacement, even if we have a result at an index, it may be
   * related to the previous query, this methods ensures the result is current.
   *
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
   * @returns {HtmlElement|XulElement} the result's element.
   */
  async waitForAutocompleteResultAt(win, index) {
    // TODO Bug 1530338: Quantum Bar doesn't yet implement lazy results replacement.
    await this.promiseSearchComplete(win);
    let container = this.getResultsContainer(win);
    if (index >= container.children.length) {
      throw new Error("Not enough results");
    }
    return container.children[index];
  },

  /**
   * Returns the oneOffSearchButtons object for the urlbar.
   *
   * @param {object} win The window containing the urlbar
   * @returns {object} The oneOffSearchButtons
   */
  getOneOffSearchButtons(win) {
    return win.gURLBar.view.oneOffSearchButtons;
  },

  /**
   * Returns a specific button of a result.
   *
   * @param {object} win The window containing the urlbar
   * @param {string} buttonName The name of the button, e.g. "menu", "0", etc.
   * @param {number} resultIndex The index of the result
   * @returns {HtmlElement} The button
   */
  getButtonForResultIndex(win, buttonName, resultIndex) {
    return this.getRowAt(win, resultIndex).querySelector(
      `.urlbarView-button-${buttonName}`
    );
  },

  /**
   * Show the result menu button regardless of the result being hovered or
   + selected.
   *
   * @param {object} win The window containing the urlbar
   */
  disableResultMenuAutohide(win) {
    let container = this.getResultsContainer(win);
    let attr = "disable-resultmenu-autohide";
    container.toggleAttribute(attr, true);
    this.registerCleanupFunction?.(() => {
      container.toggleAttribute(attr, false);
    });
  },

  /**
   * Opens the result menu of a specific result.
   *
   * @param {object} win The window containing the urlbar
   * @param {object} [options] The options object.
   * @param {number} [options.resultIndex] The index of the result. Defaults
   *        to the current selected index.
   * @param {boolean} [options.byMouse] Whether to open the menu by mouse or
   *        keyboard.
   * @param {string} [options.activationKey] Key to activate the button with,
   *        defaults to KEY_Enter.
   */
  async openResultMenu(
    win,
    {
      resultIndex = win.gURLBar.view.selectedRowIndex,
      byMouse = false,
      activationKey = "KEY_Enter",
    } = {}
  ) {
    this.Assert?.ok(win.gURLBar.view.isOpen, "view should be open");
    let menuButton = this.getButtonForResultIndex(win, "menu", resultIndex);
    this.Assert?.ok(
      menuButton,
      `found the menu button at result index ${resultIndex}`
    );
    let promiseMenuOpen = lazy.BrowserTestUtils.waitForEvent(
      win.gURLBar.view.resultMenu,
      "popupshown"
    );
    if (byMouse) {
      this.info(
        `synthesizing mousemove on row to make the menu button visible`
      );
      await this.EventUtils.promiseElementReadyForUserInput(
        menuButton.closest(".urlbarView-row"),
        win,
        this.info
      );
      this.info(`got mousemove, now clicking the menu button`);
      this.EventUtils.synthesizeMouseAtCenter(menuButton, {}, win);
      this.info(`waiting for the menu popup to open via mouse`);
    } else {
      this.info(`selecting the result at index ${resultIndex}`);
      while (win.gURLBar.view.selectedRowIndex != resultIndex) {
        this.EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
      }
      if (this.getSelectedElement(win) != menuButton) {
        this.EventUtils.synthesizeKey("KEY_Tab", {}, win);
      }
      this.Assert?.equal(
        this.getSelectedElement(win),
        menuButton,
        `selected the menu button at result index ${resultIndex}`
      );
      this.EventUtils.synthesizeKey(activationKey, {}, win);
      this.info(`waiting for ${activationKey} to open the menu popup`);
    }
    await promiseMenuOpen;
    this.Assert?.equal(
      win.gURLBar.view.resultMenu.state,
      "open",
      "Checking popup state"
    );
  },

  /**
   * Opens the result menu of a specific result and gets a menu item by either
   * accesskey or command name. Either `accesskey` or `command` must be given.
   *
   * @param {object} options
   *   The options object.
   * @param {object} options.window
   *   The window containing the urlbar.
   * @param {string} options.accesskey
   *   The access key of the menu item to return.
   * @param {string} options.command
   *   The command name of the menu item to return.
   * @param {number} options.resultIndex
   *   The index of the result. Defaults to the current selected index.
   * @param {boolean} options.openByMouse
   *   Whether to open the menu by mouse or keyboard.
   * @param {Array} options.submenuSelectors
   *   If the command is in the top-level result menu, leave this as an empty
   *   array. If it's in a submenu, set this to an array where each element i is
   *   a selector that can be used to get the i'th menu item that opens a
   *   submenu.
   */
  async openResultMenuAndGetItem({
    window,
    accesskey,
    command,
    resultIndex = window.gURLBar.view.selectedRowIndex,
    openByMouse = false,
    submenuSelectors = [],
  }) {
    await this.openResultMenu(window, { resultIndex, byMouse: openByMouse });

    // Open the sequence of submenus that contains the item.
    for (let selector of submenuSelectors) {
      let menuitem = window.gURLBar.view.resultMenu.querySelector(selector);
      if (!menuitem) {
        throw new Error("Submenu item not found for selector: " + selector);
      }

      let promisePopup = lazy.BrowserTestUtils.waitForEvent(
        window.gURLBar.view.resultMenu,
        "popupshown"
      );

      if (AppConstants.platform == "macosx") {
        // Synthesized clicks don't work in the native Mac menu.
        this.info(
          "Calling openMenu() on submenu item with selector: " + selector
        );
        menuitem.openMenu(true);
      } else {
        this.info("Clicking submenu item with selector: " + selector);
        this.EventUtils.synthesizeMouseAtCenter(menuitem, {}, window);
      }

      this.info("Waiting for submenu popupshown event");
      await promisePopup;
      this.info("Got the submenu popupshown event");
    }

    // Now get the item.
    let menuitem;
    if (accesskey) {
      await lazy.BrowserTestUtils.waitForCondition(() => {
        menuitem = window.gURLBar.view.resultMenu.querySelector(
          `menuitem[accesskey=${accesskey}]`
        );
        return menuitem;
      }, "Waiting for strings to load");
    } else if (command) {
      menuitem = window.gURLBar.view.resultMenu.querySelector(
        `menuitem[data-command=${command}]`
      );
    } else {
      throw new Error("accesskey or command must be specified");
    }

    return menuitem;
  },

  /**
   * Opens the result menu of a specific result and presses an access key to
   * activate a menu item.
   *
   * @param {object} win The window containing the urlbar
   * @param {string} accesskey The access key to press once the menu is open
   * @param {object} [options] The options object.
   * @param {number} [options.resultIndex] The index of the result. Defaults
   *        to the current selected index.
   * @param {boolean} [options.openByMouse] Whether to open the menu by mouse
   *        or keyboard.
   */
  async openResultMenuAndPressAccesskey(
    win,
    accesskey,
    {
      resultIndex = win.gURLBar.view.selectedRowIndex,
      openByMouse = false,
    } = {}
  ) {
    let menuitem = await this.openResultMenuAndGetItem({
      accesskey,
      resultIndex,
      openByMouse,
      window: win,
    });
    if (!menuitem) {
      throw new Error("Menu item not found for accesskey: " + accesskey);
    }

    let promiseCommand = lazy.BrowserTestUtils.waitForEvent(
      win.gURLBar.view.resultMenu,
      "command"
    );

    if (AppConstants.platform == "macosx") {
      // The native Mac menu doesn't support access keys.
      this.info("calling doCommand() to activate menu item");
      menuitem.doCommand();
      win.gURLBar.view.resultMenu.hidePopup(true);
    } else {
      this.info(`pressing access key (${accesskey}) to activate menu item`);
      this.EventUtils.synthesizeKey(accesskey, {}, win);
    }

    this.info("waiting for command event");
    await promiseCommand;
    this.info("got the command event");
  },

  /**
   * Opens the result menu of a specific result and clicks a menu item with a
   * specified command name.
   *
   * @param {object} win
   *   The window containing the urlbar.
   * @param {string|Array} commandOrArray
   *   If the command is in the top-level result menu, set this to the command
   *   name. If it's in a submenu, set this to an array where each element i is
   *   a selector that can be used to click the i'th menu item that opens a
   *   submenu, and the last element is the command name.
   * @param {object} options
   *   The options object.
   * @param {number} options.resultIndex
   *   The index of the result. Defaults to the current selected index.
   * @param {boolean} options.openByMouse
   *   Whether to open the menu by mouse or keyboard.
   */
  async openResultMenuAndClickItem(
    win,
    commandOrArray,
    {
      resultIndex = win.gURLBar.view.selectedRowIndex,
      openByMouse = false,
    } = {}
  ) {
    let submenuSelectors = Array.isArray(commandOrArray)
      ? commandOrArray
      : [commandOrArray];
    let command = submenuSelectors.pop();

    let menuitem = await this.openResultMenuAndGetItem({
      resultIndex,
      openByMouse,
      command,
      submenuSelectors,
      window: win,
    });
    if (!menuitem) {
      throw new Error("Menu item not found for command: " + command);
    }

    let promiseCommand = lazy.BrowserTestUtils.waitForEvent(
      win.gURLBar.view.resultMenu,
      "command"
    );

    if (AppConstants.platform == "macosx") {
      // Synthesized clicks don't work in the native Mac menu.
      this.info("calling doCommand() to activate menu item");
      menuitem.doCommand();
      win.gURLBar.view.resultMenu.hidePopup(true);
    } else {
      this.info("Clicking menu item with command: " + command);
      this.EventUtils.synthesizeMouseAtCenter(menuitem, {}, win);
    }

    this.info("Waiting for command event");
    await promiseCommand;
    this.info("Got the command event");
  },

  /**
   * Returns true if the oneOffSearchButtons are visible.
   *
   * @param {object} win The window containing the urlbar
   * @returns {boolean} True if the buttons are visible.
   */
  getOneOffSearchButtonsVisible(win) {
    let buttons = this.getOneOffSearchButtons(win);
    return buttons.style.display != "none" && !buttons.container.hidden;
  },

  /**
   * Gets an abstracted representation of the result at an index.
   *
   * @param {object} win The window containing the urlbar
   * @param {number} index The index to look for
   * @returns {object} An object with numerous properties describing the result.
   */
  async getDetailsOfResultAt(win, index) {
    let element = await this.waitForAutocompleteResultAt(win, index);
    let details = {};
    let result = element.result;
    details.result = result;
    let { url, postData } = UrlbarUtils.getUrlFromResult(result);
    details.url = url;
    details.postData = postData;
    details.type = result.type;
    details.source = result.source;
    details.heuristic = result.heuristic;
    details.autofill = !!result.autofill;
    details.image =
      element.getElementsByClassName("urlbarView-favicon")[0]?.src;
    details.title = result.title;
    details.tags = "tags" in result.payload ? result.payload.tags : [];
    details.isSponsored = result.payload.isSponsored;
    let actions = element.getElementsByClassName("urlbarView-action");
    let urls = element.getElementsByClassName("urlbarView-url");
    let typeIcon = element.querySelector(".urlbarView-type-icon");
    await win.document.l10n.translateFragment(element);
    details.displayed = {
      title: element.getElementsByClassName("urlbarView-title")[0]?.textContent,
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
    } else if (details.type == UrlbarUtils.RESULT_TYPE.DYNAMIC) {
      details.dynamicType = result.payload.dynamicType;
    }
    return details;
  },

  /**
   * Gets the currently selected element.
   *
   * @param {object} win The window containing the urlbar.
   * @returns {HtmlElement|XulElement} The selected element.
   */
  getSelectedElement(win) {
    return win.gURLBar.view.selectedElement || null;
  },

  /**
   * Gets the index of the currently selected element.
   *
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected index.
   */
  getSelectedElementIndex(win) {
    return win.gURLBar.view.selectedElementIndex;
  },

  /**
   * Gets the row at a specific index.
   *
   * @param {object} win The window containing the urlbar.
   * @param {number} index The index to look for.
   * @returns {HTMLElement|XulElement} The selected row.
   */
  getRowAt(win, index) {
    return this.getResultsContainer(win).children.item(index);
  },

  /**
   * Gets the currently selected row. If the selected element is a descendant of
   * a row, this will return the ancestor row.
   *
   * @param {object} win The window containing the urlbar.
   * @returns {HTMLElement|XulElement} The selected row.
   */
  getSelectedRow(win) {
    return this.getRowAt(win, this.getSelectedRowIndex(win));
  },

  /**
   * Gets the index of the currently selected element.
   *
   * @param {object} win The window containing the urlbar.
   * @returns {number} The selected row index.
   */
  getSelectedRowIndex(win) {
    return win.gURLBar.view.selectedRowIndex;
  },

  /**
   * Selects the element at the index specified.
   *
   * @param {object} win The window containing the urlbar.
   * @param {index} index The index to select.
   */
  setSelectedRowIndex(win, index) {
    win.gURLBar.view.selectedRowIndex = index;
  },

  getResultsContainer(win) {
    return win.gURLBar.view.panel.querySelector(".urlbarView-results");
  },

  /**
   * Gets the number of results.
   * You must wait for the query to be complete before using this.
   *
   * @param {object} win The window containing the urlbar
   * @returns {number} the number of results.
   */
  getResultCount(win) {
    return this.getResultsContainer(win).children.length;
  },

  /**
   * Ensures at least one search suggestion is present.
   *
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
   *
   * @param {object} httpserver an HTTP Server instance
   * @param {number} count Number of connections to wait for
   * @returns {Promise} resolved when all the expected connections were started.
   */
  promiseSpeculativeConnections(httpserver, count) {
    if (!httpserver) {
      throw new Error("Must provide an http server");
    }
    return lazy.BrowserTestUtils.waitForCondition(
      () => httpserver.connectionNumber == count,
      "Waiting for speculative connection setup"
    );
  },

  /**
   * Waits for the popup to be shown.
   *
   * @param {object} win The window containing the urlbar
   * @param {Function} openFn Function to be used to open the popup.
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
    this.info("Waiting for the urlbar view to open");
    await new Promise(resolve => {
      win.gURLBar.controller.addQueryListener({
        onViewOpen() {
          win.gURLBar.controller.removeQueryListener(this);
          resolve();
        },
      });
    });
    this.info("Urlbar view opened");
  },

  /**
   * Waits for the popup to be hidden.
   *
   * @param {object} win The window containing the urlbar
   * @param {Function} [closeFn] Function to be used to close the popup, if not
   *        supplied it will default to a closing the popup directly.
   * @returns {Promise} resolved once the popup is closed
   */
  async promisePopupClose(win, closeFn = null) {
    let closePromise = new Promise(resolve => {
      if (!win.gURLBar.view.isOpen) {
        resolve();
        return;
      }
      win.gURLBar.controller.addQueryListener({
        onViewClose() {
          win.gURLBar.controller.removeQueryListener(this);
          resolve();
        },
      });
    });
    if (closeFn) {
      this.info("Awaiting custom close function");
      await closeFn();
      this.info("Done awaiting custom close function");
    } else {
      this.info("Closing the view directly");
      win.gURLBar.view.close();
    }
    this.info("Waiting for the view to close");
    await closePromise;
    this.info("Urlbar view closed");
  },

  /**
   * Open the input field context menu and run a task on it.
   *
   * @param {nsIWindow} win the current window
   * @param {Function} task a task function to run, gets the contextmenu popup
   *        as argument.
   */
  async withContextMenu(win, task) {
    let textBox = win.gURLBar.querySelector("moz-input-box");
    let cxmenu = textBox.menupopup;
    let openPromise = lazy.BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
    this.EventUtils.synthesizeMouseAtCenter(
      win.gURLBar.inputField,
      {
        type: "contextmenu",
        button: 2,
      },
      win
    );
    await openPromise;
    // On Mac sometimes the menuitems are not ready.
    await new Promise(win.requestAnimationFrame);
    try {
      await task(cxmenu);
    } finally {
      // Close the context menu if the task didn't pick anything.
      if (cxmenu.state == "open" || cxmenu.state == "showing") {
        let closePromise = lazy.BrowserTestUtils.waitForEvent(
          cxmenu,
          "popuphidden"
        );
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
   * Asserts that the input is in a given search mode, or no search mode. Can
   * only be used if UrlbarTestUtils has been initialized with init().
   *
   * @param {Window} window
   *   The browser window.
   * @param {object} expectedSearchMode
   *   The expected search mode object.
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

    if (
      window.gURLBar.searchMode?.source &&
      window.gURLBar.searchMode.source !== UrlbarUtils.RESULT_SOURCE.SEARCH
    ) {
      this.Assert.equal(
        window.gURLBar.getAttribute("searchmodesource"),
        UrlbarUtils.getResultSourceName(window.gURLBar.searchMode.source),
        "gURLBar has proper searchmodesource attribute"
      );
    } else {
      this.Assert.ok(
        !window.gURLBar.hasAttribute("searchmodesource"),
        "gURLBar does not have searchmodesource attribute"
      );
    }

    if (!expectedSearchMode) {
      // Check the input's placeholder.
      const prefName =
        "browser.urlbar.placeholderName" +
        (lazy.PrivateBrowsingUtils.isWindowPrivate(window) ? ".private" : "");
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

    let isGeneralPurposeEngine = false;
    if (expectedSearchMode.engineName) {
      let engine = Services.search.getEngineByName(
        expectedSearchMode.engineName
      );
      isGeneralPurposeEngine = engine.isGeneralPurposeEngine;
      expectedSearchMode.isGeneralPurposeEngine = isGeneralPurposeEngine;
    }

    // expectedSearchMode may come from UrlbarUtils.LOCAL_SEARCH_MODES.  The
    // objects in that array include useful metadata like icon URIs and pref
    // names that are not usually included in actual search mode objects.  For
    // convenience, ignore those properties if they aren't also present in the
    // urlbar's actual search mode object.
    let ignoreProperties = ["icon", "pref", "restrict", "telemetryLabel"];
    for (let prop of ignoreProperties) {
      if (prop in expectedSearchMode && !(prop in window.gURLBar.searchMode)) {
        this.info(
          `Ignoring unimportant property '${prop}' in expected search mode`
        );
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
        id: isGeneralPurposeEngine
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
          let engineRootDomain =
            lazy.UrlbarSearchUtils.getRootDomainFromEngine(engine);
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
   * before you call this. Can only be used if UrlbarTestUtils has been
   * initialized with init().
   *
   * @param {object} window
   *   The window to operate on.
   * @param {object} searchMode
   *   If given, the one-off matching this search mode will be clicked; it
   *   should be a full search mode object as described in
   *   UrlbarInput.setSearchMode.  If not given, the first one-off is clicked.
   */
  async enterSearchMode(window, searchMode = null) {
    this.info(`Enter Search Mode ${JSON.stringify(searchMode)}`);

    // Ensure any pending query is complete.
    await this.promiseSearchComplete(window);

    // Ensure the the one-offs are finished rebuilding and visible.
    let oneOffs = this.getOneOffSearchButtons(window);
    await lazy.TestUtils.waitForCondition(
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
      let engine = Services.search.getEngineByName(searchMode.engineName);
      if (engine.isGeneralPurposeEngine) {
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
   * Removes the scheme from an url according to user prefs.
   *
   * @param {string} url
   *  The url that is supposed to be sanitizied.
   * @param {{removeSingleTrailingSlash: (boolean)}} options
   *    removeSingleTrailingSlash: Remove trailing slash, when trimming enabled.
   * @returns {string}
   *  The sanitized URL.
   */
  trimURL(url, { removeSingleTrailingSlash = true } = {}) {
    if (!lazy.UrlbarPrefs.get("trimURLs")) {
      return url;
    }

    let sanitizedURL = url;
    if (removeSingleTrailingSlash) {
      sanitizedURL =
        lazy.BrowserUIUtils.removeSingleTrailingSlashFromURL(sanitizedURL);
    }

    if (lazy.UrlbarPrefs.get("trimHttps")) {
      sanitizedURL = sanitizedURL.replace("https://", "");
    } else {
      sanitizedURL = sanitizedURL.replace("http://", "");
    }

    // Remove empty emphasis markers in case the protocol was trimmed.
    sanitizedURL = sanitizedURL.replace("<>", "");

    return sanitizedURL;
  },

  /**
   * Returns the trimmed protocol with slashes.
   *
   * @returns {string} The trimmed protocol including slashes. Returns an empty
   *                   string, when the protocol trimming is disabled.
   */
  getTrimmedProtocolWithSlashes() {
    if (Services.prefs.getBoolPref("browser.urlbar.trimURLs")) {
      return Services.prefs.getBoolPref("browser.urlbar.trimHttps")
        ? "https://"
        : "http://"; // eslint-disable-this-line @microsoft/sdl/no-insecure-url
    }
    return "";
  },

  /**
   * Exits search mode. If neither `backspace` nor `clickClose` is given, we'll
   * default to backspacing. Can only be used if UrlbarTestUtils has been
   * initialized with init().
   *
   * @param {object} window
   *   The window to operate on.
   * @param {object} options
   *   Options object
   * @param {boolean} options.backspace
   *   Exits search mode by backspacing at the beginning of the search string.
   * @param {boolean} options.clickClose
   *   Exits search mode by clicking the close button on the search mode
   *   indicator.
   * @param {boolean} [options.waitForSearch]
   *   Whether the test should wait for a search after exiting search mode.
   *   Defaults to true.
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
   *
   * @param {object} win The browser window
   * @returns {Promise<number>}
   *   resolved when fetching is complete. Its value is a userContextId
   */
  async promiseUserContextId(win) {
    const defaultId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
    let context = await win.gURLBar.lastQueryContextPromise;
    return context.userContextId || defaultId;
  },

  /**
   * Dispatches an input event to the input field.
   *
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
   *
   * @param {object} options Additional options to pass to the UrlbarController
   *        constructor.
   * @returns {UrlbarController} A new controller.
   */
  newMockController(options = {}) {
    return new lazy.UrlbarController(
      Object.assign(
        {
          input: {
            isPrivate: false,
            onFirstResult() {
              return false;
            },
            getSearchSource() {
              return "dummy-search-source";
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
      await lazy.AddonTestUtils.promiseStartupManager();
    } catch (error) {
      if (!error.message.includes("already started")) {
        throw error;
      }
    }
  },

  /**
   * Enrolls in a mock Nimbus feature.
   *
   * If you call UrlbarPrefs.updateFirefoxSuggestScenario() from an xpcshell
   * test, you must call this first to intialize the Nimbus urlbar feature.
   *
   * @param {object} value
   *   Define any desired Nimbus variables in this object.
   * @param {string} [feature]
   *   The feature to init.
   * @param {string} [enrollmentType]
   *   The enrollment type, either "rollout" (default) or "config".
   * @returns {Function}
   *   A cleanup function that will unenroll the feature, returns a promise.
   */
  async initNimbusFeature(
    value = {},
    feature = "urlbar",
    enrollmentType = "rollout"
  ) {
    this.info("initNimbusFeature awaiting ExperimentManager.onStartup");
    await lazy.ExperimentManager.onStartup();

    this.info("initNimbusFeature awaiting ExperimentAPI.ready");
    await lazy.ExperimentAPI.ready();

    let method =
      enrollmentType == "rollout"
        ? "enrollWithRollout"
        : "enrollWithFeatureConfig";
    this.info(`initNimbusFeature awaiting ExperimentFakes.${method}`);
    let doCleanup = await lazy.ExperimentFakes[method]({
      featureId: lazy.NimbusFeatures[feature].featureId,
      value: { enabled: true, ...value },
    });

    this.info("initNimbusFeature done");

    this.registerCleanupFunction?.(async () => {
      // If `doCleanup()` has already been called (i.e., by the caller), it will
      // throw an error here.
      try {
        await doCleanup();
      } catch (error) {}
    });

    return doCleanup;
  },

  /**
   * Simulate that user clicks URLBar and inputs text into it.
   *
   * @param {object} win
   *   The browser window containing target gURLBar.
   * @param {string} text
   *   The text to be input.
   */
  async inputIntoURLBar(win, text) {
    if (win.gURLBar.focused) {
      win.gURLBar.select();
    } else {
      this.EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
      await lazy.TestUtils.waitForCondition(() => win.gURLBar.focused);
    }
    if (text.length > 1) {
      // Set most of the string directly instead of going through sendString,
      // so that we don't make life unnecessarily hard for consumers by
      // possibly starting multiple searches.
      win.gURLBar._setValue(
        text.substr(0, text.length - 1),
        false /* allowTrim = */
      );
    }
    this.EventUtils.sendString(text.substr(-1, 1), win);
  },

  /**
   * Checks the urlbar value fomatting for a given URL.
   *
   * @param {window} win
   *   The input in this window will be tested.
   * @param {string} urlFormatString
   *   The URL to test. The parts the are expected to be de-emphasized should be
   *   wrapped in "<" and ">" chars.
   * @param {object} [options]
   *   Options object.
   * @param {string} [options.clobberedURLString]
   *      Normally the URL is de-emphasized in-place, thus it's enough to pass
   *      urlString. In some cases however the formatter may decide to replace
   *      the URL with a fixed one, because it can't properly guess a host. In
   *      that case clobberedURLString is the expected de-emphasized value. The
   *      parts the are expected to be de-emphasized should be wrapped in "<"
   *      and ">" chars.
   * @param {string} [options.additionalMsg]
   *   Additional message to use for Assert.equal.
   * @param {int} [options.selectionType]
   *   The selectionType for which the input should be checked.
   */
  checkFormatting(
    win,
    urlFormatString,
    {
      clobberedURLString = null,
      additionalMsg = null,
      selectionType = Ci.nsISelectionController.SELECTION_URLSECONDARY,
    } = {}
  ) {
    let selectionController = win.gURLBar.editor.selectionController;
    let selection = selectionController.getSelection(selectionType);
    let value = win.gURLBar.editor.rootElement.textContent;
    let result = "";
    for (let i = 0; i < selection.rangeCount; i++) {
      let range = selection.getRangeAt(i).toString();
      let pos = value.indexOf(range);
      result += value.substring(0, pos) + "<" + range + ">";
      value = value.substring(pos + range.length);
    }
    result += value;
    this.Assert.equal(
      result,
      clobberedURLString || urlFormatString,
      "Correct part of the URL is de-emphasized" +
        (additionalMsg ? ` (${additionalMsg})` : "")
    );
  },
};

UrlbarTestUtils.formHistory = {
  /**
   * Adds values to the urlbar's form history.
   *
   * @param {Array} values
   *   The form history entries to remove.
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  add(values = [], window = lazy.BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return lazy.FormHistoryTestUtils.add(fieldname, values);
  },

  /**
   * Removes values from the urlbar's form history.  If you want to remove all
   * history, use clearFormHistory.
   *
   * @param {Array} values
   *   The form history entries to remove.
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  remove(values = [], window = lazy.BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return lazy.FormHistoryTestUtils.remove(fieldname, values);
  },

  /**
   * Removes all values from the urlbar's form history.  If you want to remove
   * individual values, use removeFormHistory.
   *
   * @param {object} window
   *   The window containing the urlbar.
   * @returns {Promise} resolved once the operation is complete.
   */
  clear(window = lazy.BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return lazy.FormHistoryTestUtils.clear(fieldname);
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
  search(criteria = {}, window = lazy.BrowserWindowTracker.getTopWindow()) {
    let fieldname = this.getFormHistoryName(window);
    return lazy.FormHistoryTestUtils.search(fieldname, criteria);
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
    return lazy.TestUtils.topicObserved(
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
  getFormHistoryName(window = lazy.BrowserWindowTracker.getTopWindow()) {
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
   * @param {object} options
   *   Constructor options
   * @param {Array} options.results
   *   An array of UrlbarResult objects that will be the provider's results.
   * @param {string} [options.name]
   *   The provider's name.  Provider names should be unique.
   * @param {UrlbarUtils.PROVIDER_TYPE} [options.type]
   *   The provider's type.
   * @param {number} [options.priority]
   *   The provider's priority.  Built-in providers have a priority of zero.
   * @param {number} [options.addTimeout]
   *   If non-zero, each result will be added on this timeout.  If zero, all
   *   results will be added immediately and synchronously.
   *   If there's no results, the query will be completed after this timeout.
   * @param {Function} [options.onCancel]
   *   If given, a function that will be called when the provider's cancelQuery
   *   method is called.
   * @param {Function} [options.onSelection]
   *   If given, a function that will be called when
   *   {@link UrlbarView.#selectElement} method is called.
   * @param {Function} [options.onEngagement]
   *   If given, a function that will be called when engagement.
   * @param {Function} [options.delayResultsPromise]
   *   If given, we'll await on this before returning results.
   */
  constructor({
    results,
    name = "TestProvider" + Services.uuid.generateUUID(),
    type = UrlbarUtils.PROVIDER_TYPE.PROFILE,
    priority = 0,
    addTimeout = 0,
    onCancel = null,
    onSelection = null,
    onEngagement = null,
    delayResultsPromise = null,
  } = {}) {
    if (delayResultsPromise && addTimeout) {
      throw new Error(
        "Can't provide both `addTimeout` and `delayResultsPromise`"
      );
    }
    super();
    this._results = results;
    this._name = name;
    this._type = type;
    this._priority = priority;
    this._addTimeout = addTimeout;
    this._onCancel = onCancel;
    this._onSelection = onSelection;
    this._onEngagement = onEngagement;
    this._delayResultsPromise = delayResultsPromise;
    // As this has been a common source of mistakes, auto-upgrade the provider
    // type to heuristic if any result is heuristic.
    if (!type && this._results?.some(r => r.heuristic)) {
      this._type = UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
    }
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
    if (!this._results.length && this._addTimeout) {
      await new Promise(resolve => lazy.setTimeout(resolve, this._addTimeout));
    }
    if (this._delayResultsPromise) {
      await this._delayResultsPromise;
    }
    for (let result of this._results) {
      if (!this._addTimeout) {
        addCallback(this, result);
      } else {
        await new Promise(resolve => {
          lazy.setTimeout(() => {
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

  onEngagement(state, queryContext, details, controller) {
    if (this._onEngagement) {
      this._onEngagement(state, queryContext, details, controller);
    }
  }
}

UrlbarTestUtils.TestProvider = TestProvider;
