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
   * This is a reference to the interaction for the foreground browser. If the
   * application is not focussed, or a non-interaction page is visible, then
   * this will be undefined.
   *
   * @type {InteractionInfo}
   */
  #currentInteraction = undefined;

  /**
   * This stores the time that the current interaction was activated or last
   * updated. It is used to calculate the total page view time.
   *
   * @type {number}
   */
  _lastUpdateTime = Cu.now();

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

    if (docInfo.isActive) {
      this.#currentInteraction = interaction;
      this._lastUpdateTime = Cu.now();
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
    let interaction = this.#interactions.get(browser);
    if (!interaction) {
      return;
    }

    this.#updateInteractionViewTime();
    this.logConsole.debug("End of interaction for", interaction);

    this._updateDatabase(interaction);

    this.#interactions.delete(browser);
    this.#currentInteraction = undefined;
  }

  /**
   * Updates the interaction view time of the current interaction.
   */
  #updateInteractionViewTime() {
    if (!this.#currentInteraction) {
      return;
    }
    let now = Cu.now();
    this.#currentInteraction.totalViewTime += now - this._lastUpdateTime;
    this._lastUpdateTime = now;
  }

  /**
   * Handles the TabSelect notification. Updates the current interaction and
   * then switches it to the interaction for the new tab. The new interaction
   * may be null if it doesn't exist.
   *
   * @param {Browser} previousBrowser
   *   The instance of the browser that the user switched away from.
   * @param {Browser} browser
   *   The instance of the browser that the user switched to.
   */
  #onTabSelect(previousBrowser, browser) {
    this.logConsole.debug("Tab switch notified");

    let interaction = this.#interactions.get(previousBrowser);
    if (interaction) {
      this.#updateInteractionViewTime();
      // The interaction might continue later, but for now we update the
      // database to ensure that the interaction does not go away (e.g. on
      // tab close).
      this._updateDatabase(interaction);
    }

    // Note: this could be undefined in the case of switching to a browser with
    // something that we don't track, e.g. about:blank.
    this.#currentInteraction = this.#interactions.get(browser);
    this._lastUpdateTime = Cu.now();
  }

  /**
   * Handles various events and forwards them to appropriate functions.
   *
   * @param {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "TabSelect":
        this.#onTabSelect(
          event.detail.previousTab.linkedBrowser,
          event.target.linkedBrowser
        );
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
    win.addEventListener("TabSelect", this, true);
  }

  /**
   * Handles removing of listeners from a window.
   *
   * @param {DOMWindow} win
   *   The window to remove listeners from.
   */
  #unregisterWindow(win) {
    win.removeEventListener("TabSelect", this, true);
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
