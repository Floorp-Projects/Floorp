/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Interactions"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const DOMWINDOW_OPENED_TOPIC = "domwindowopened";

/**
 * @typedef {object} DocumentInfo
 *   DocumentInfo is used to pass document information from the child process
 *   to _Interactions.
 * @property {boolean} isActive
 *   Set to true if the document is active, i.e. visible.
 * @property {string} url
 *   The url of the page that was interacted with.
 */

/**
 * @typedef {object} InteractionInfo
 *   InteractionInfo is used to store information associated with interactions.
 * @property {number} totalViewTime
 *   Time in milliseconds that the page has been actively viewed for.
 * @property {string} url
 *   The url of the page that was interacted with.
 */

/**
 * The Interactions object sets up listeners and other approriate tools for
 * obtaining interaction information and passing it to the InteractionsManager.
 */
class _Interactions {
  /**
   * This is used to store potential interactions. It maps the browser
   * to the current interaction information.
   * The current interaction is updated to the database when it transitions
   * to non-active, which occurs before a browser tab is closed, hence this
   * can be a weak map.
   *
   * @type {WeakMap<browser, InteractionInfo>}
   */
  #interactions = new WeakMap();

  /**
   * Tracks the currently active window so that we can avoid recording
   * interactions in non-active windows.
   *
   * @type {DOMWindow}
   */
  #activeWindow = undefined;

  /**
   * This stores the page view start time of the current page view.
   * For any single page view, this may be moved multiple times as the
   * associated interaction is updated for the current total page view time.
   *
   * @type {number}
   */
  _pageViewStartTime = Cu.now();

  /**
   * Initializes, sets up actors and observers.
   */
  init() {
    if (
      !Services.prefs.getBoolPref("browser.places.interactions.enabled", false)
    ) {
      return;
    }

    this.logConsole = console.createInstance({
      prefix: "InteractionsManager",
      maxLogLevel: Services.prefs.getBoolPref(
        "browser.places.interactions.log",
        false
      )
        ? "Debug"
        : "Warn",
    });
    this.logConsole.debug("init");

    ChromeUtils.registerWindowActor("Interactions", {
      parent: {
        moduleURI: "resource:///actors/InteractionsParent.jsm",
      },
      child: {
        moduleURI: "resource:///actors/InteractionsChild.jsm",
        group: "browsers",
        events: {
          DOMContentLoaded: {},
          pagehide: { mozSystemGroup: true },
        },
      },
    });

    this.#activeWindow = Services.wm.getMostRecentBrowserWindow();

    for (let win of BrowserWindowTracker.orderedWindows) {
      if (!win.closed) {
        this.#registerWindow(win);
      }
    }
    Services.obs.addObserver(this, DOMWINDOW_OPENED_TOPIC, true);
  }

  /**
   * Registers the start of a new interaction.
   *
   * @param {Browser} browser
   *   The browser object associated with the interaction.
   * @param {DocumentInfo} docInfo
   *   The document information of the page associated with the interaction.
   */
  registerNewInteraction(browser, docInfo) {
    let interaction = this.#interactions.get(browser);
    if (interaction && interaction.url != docInfo.url) {
      this.registerEndOfInteraction(browser);
    }

    this.logConsole.debug("New interaction", docInfo);
    interaction = {
      url: docInfo.url,
      totalViewTime: 0,
    };
    this.#interactions.set(browser, interaction);

    // Only reset the time if this is being loaded in the active tab of the
    // active window.
    if (docInfo.isActive && browser.ownerGlobal == this.#activeWindow) {
      this._pageViewStartTime = Cu.now();
    }
  }

  /**
   * Registers the end of an interaction, e.g. if the user navigates away
   * from the page. This will store the final interaction details and clear
   * the current interaction.
   *
   * @param {Browser} browser
   *   The browser object associated with the interaction.
   */
  registerEndOfInteraction(browser) {
    this.logConsole.debug("End of interaction");

    this.#updateInteraction(browser);
    this.#interactions.delete(browser);
  }

  /**
   * Updates the current interaction.
   *
   * @param {Browser} [browser]
   *   The browser object that has triggered the update, if known. This is
   *   used to check if the browser is in the active window, and as an
   *   optimization to avoid obtaining the browser object.
   */
  #updateInteraction(browser = undefined) {
    if (
      !this.#activeWindow ||
      (browser && browser.ownerGlobal != this.#activeWindow)
    ) {
      this.logConsole.debug("No active window");
      return;
    }

    if (!browser) {
      browser = this.#activeWindow.gBrowser.selectedTab.linkedBrowser;
    }

    let interaction = this.#interactions.get(browser);
    if (!interaction) {
      this.logConsole.debug("No interaction to update");
      return;
    }

    interaction.totalViewTime += Cu.now() - this._pageViewStartTime;
    this._pageViewStartTime = Cu.now();
    this._updateDatabase(interaction);
  }

  /**
   * Handles a window becoming active.
   *
   * @param {DOMWindow} win
   */
  #onActivateWindow(win) {
    this.logConsole.debug("Activate window");

    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return;
    }

    this.#activeWindow = win;
    this._pageViewStartTime = Cu.now();
  }

  /**
   * Handles a window going inactive.
   *
   * @param {DOMWindow} win
   */
  #onDeactivateWindow(win) {
    this.logConsole.debug("Deactivate window");

    this.#updateInteraction();
    this.#activeWindow = undefined;
  }

  /**
   * Handles the TabSelect notification. Updates the current interaction and
   * then switches it to the interaction for the new tab. The new interaction
   * may be null if it doesn't exist.
   *
   * @param {Browser} previousBrowser
   *   The instance of the browser that the user switched away from.
   */
  #onTabSelect(previousBrowser) {
    this.logConsole.debug("Tab switch notified");

    this.#updateInteraction(previousBrowser);
    this._pageViewStartTime = Cu.now();
  }

  /**
   * Handles various events and forwards them to appropriate functions.
   *
   * @param {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "TabSelect":
        this.#onTabSelect(event.detail.previousTab.linkedBrowser);
        break;
      case "activate":
        this.#onActivateWindow(event.target);
        break;
      case "deactivate":
        this.#onDeactivateWindow(event.target);
        break;
      case "unload":
        this.#unregisterWindow(event.target);
        break;
    }
  }

  /**
   * Handles notifications from the observer service.
   *
   * @param {nsISupports} subject
   * @param {string} topic
   * @param {string} data
   */
  observe(subject, topic, data) {
    switch (topic) {
      case DOMWINDOW_OPENED_TOPIC:
        this.#onWindowOpen(subject);
        break;
    }
  }

  /**
   * Handles registration of listeners in a new window.
   *
   * @param {DOMWindow} win
   *   The window to register in.
   */
  #registerWindow(win) {
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return;
    }

    win.addEventListener("TabSelect", this, true);
    win.addEventListener("deactivate", this, true);
    win.addEventListener("activate", this, true);
  }

  /**
   * Handles removing of listeners from a window.
   *
   * @param {DOMWindow} win
   *   The window to remove listeners from.
   */
  #unregisterWindow(win) {
    win.removeEventListener("TabSelect", this, true);
    win.removeEventListener("deactivate", this, true);
    win.removeEventListener("activate", this, true);
  }

  /**
   * Handles a new window being opened, waits for load and checks that
   * it is a browser window, then adds listeners.
   *
   * @param {DOMWindow} win
   *   The window being opened.
   */
  #onWindowOpen(win) {
    win.addEventListener(
      "load",
      () => {
        if (
          win.document.documentElement.getAttribute("windowtype") !=
          "navigator:browser"
        ) {
          return;
        }
        this.#registerWindow(win);
      },
      { once: true }
    );
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]);

  /**
   * Temporary test-only function to allow testing what we write to the
   * database is correct, whilst we haven't got a database.
   *
   * @param {InteractionInfo} interactionInfo
   *   The document information to write.
   */
  async _updateDatabase({ url, totalViewTime }) {}
}

const Interactions = new _Interactions();
