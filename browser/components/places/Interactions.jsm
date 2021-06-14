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

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "InteractionsManager",
    maxLogLevel: Services.prefs.getBoolPref(
      "browser.places.interactions.log",
      false
    )
      ? "Debug"
      : "Warn",
  });
});

XPCOMUtils.defineLazyServiceGetters(this, {
  idleService: ["@mozilla.org/widget/useridleservice;1", "nsIUserIdleService"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pageViewIdleTime",
  "browser.places.interactions.pageViewIdleTime",
  60
);

const DOMWINDOW_OPENED_TOPIC = "domwindowopened";

/**
 * The TypingInteraction object measures time spent typing on the current interaction.
 * This is consists of the current typing metrics as well as accumulated typing metrics.
 */
class TypingInteraction {
  /**
   * The time, in milliseconds, at which the user started typing in the current typing sequence.
   *
   * @type {number}
   */
  #typingStartTime = null;
  /**
   * The time, in milliseconds, at which the last keypress was made in the current typing sequence.
   *
   * @type {number}
   */
  #typingEndTime = null;
  /**
   * The number of keypresses made in the current typing sequence.
   *
   * @type {number}
   */
  #keypresses = 0;

  /**
   * Time, in milliseconds, after the last keypress after which we consider the current typing sequence to have ended.
   *
   * @type {number}
   */
  static _TYPING_TIMEOUT = 3000;

  /**
   * The number of keypresses accumulated in this interaction.
   * Each typing sequence will contribute to accumulated keypresses.
   *
   * @type {number}
   */
  #accumulatedKeypresses = 0;

  /**
   * Typing time, in milliseconds, accumulated in this interaction.
   * Each typing sequence will contribute to accumulated typing time.
   *
   * @type {number}
   */
  #accumulatedTypingTime = 0;

  /**
   * Adds a system event listener to the given window.
   *
   * @param {DOMWindow} win
   *   The window to register in.
   */
  registerWindow(win) {
    Services.els.addSystemEventListener(
      win.document,
      "keyup",
      this,
      /* useCapture */ false
    );
  }

  /**
   * Removes system event listener from the given window.
   *
   * @param {DOMWindow} win
   *   The window to removed listeners from
   */
  unregisterWindow(win) {
    Services.els.removeSystemEventListener(
      win.document,
      "keyup",
      this,
      /* useCapture */ false
    );
  }

  /**
   * Determines if the given key stroke is considered typing.
   *
   * @param {string} code
   * @returns {boolean} whether the key code is considered typing
   *
   */
  #isTypingKey(code) {
    if (["Space", "Comma", "Period", "Quote"].includes(code)) {
      return true;
    }

    return (
      code.startsWith("Key") ||
      code.startsWith("Digit") ||
      code.startsWith("Numpad")
    );
  }

  /**
   * Reset current typing metrics.
   */
  #resetCurrentTypingMetrics() {
    this.#keypresses = 0;
    this.#typingStartTime = null;
    this.#typingEndTime = null;
  }

  /**
   * Reset all typing interaction metrics, included those accumulated.
   */
  resetTypingInteraction() {
    this.#resetCurrentTypingMetrics();
    this.#accumulatedKeypresses = 0;
    this.#accumulatedTypingTime = 0;
  }

  /**
   * Returns an object with all current and accumulated typing metrics.
   *
   * @returns {object} with properties typingTime, keypresses
   */
  getTypingInteraction() {
    let typingInteraction = this.#getCurrentTypingMetrics();

    typingInteraction.typingTime += this.#accumulatedTypingTime;
    typingInteraction.keypresses += this.#accumulatedKeypresses;

    return typingInteraction;
  }

  /**
   * Returns an object with the current typing metrics.
   *
   * @returns {object} with properties typingTime, keypresses
   */
  #getCurrentTypingMetrics() {
    let typingInteraction = { typingTime: 0, keypresses: 0 };

    // We don't consider a single keystroke to be typing, not least because it would have 0 typing
    // time which would equate to infinite keystrokes per minute.
    if (this.#keypresses > 1) {
      let typingTime = this.#typingEndTime - this.#typingStartTime;
      typingInteraction.typingTime = typingTime;
      typingInteraction.keypresses = this.#keypresses;
    }

    return typingInteraction;
  }

  /**
   * The user has stopped typing, accumulate the current metrics and reset the counters.
   *
   */
  #onTypingEnded() {
    let typingInteraction = this.#getCurrentTypingMetrics();

    this.#accumulatedTypingTime += typingInteraction.typingTime;
    this.#accumulatedKeypresses += typingInteraction.keypresses;

    this.#resetCurrentTypingMetrics();
  }

  /**
   * Handles received window events to detect keypresses.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    switch (event.type) {
      case "keyup":
        if (
          event.target.ownerGlobal.gBrowser?.selectedBrowser == event.target &&
          this.#isTypingKey(event.code)
        ) {
          const now = Cu.now();
          // Detect typing end from previous keystroke
          const lastKeyDelay = now - this.#typingEndTime;
          if (
            this.#keypresses > 0 &&
            lastKeyDelay > TypingInteraction._TYPING_TIMEOUT
          ) {
            this.#onTypingEnded();
          }

          this.#keypresses++;
          if (!this.#typingStartTime) {
            this.#typingStartTime = now;
          }

          this.#typingEndTime = now;
        }
        break;
    }
  }
}

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
 * @property {number} typingTime
 *   Time in milliseconds that the user typed on the page
 * @property {number} keypresses
 *   The number of keypresses made on the page
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
   * This tracks and reports the typing interactions
   *
   * @type {TypingInteraction}
   */
  #typingInteraction = new TypingInteraction();

  /**
   * Tracks the currently active window so that we can avoid recording
   * interactions in non-active windows.
   *
   * @type {DOMWindow}
   */
  #activeWindow = undefined;

  /**
   * Tracks if the user is idle.
   *
   * @type {boolean}
   */
  #userIsIdle = false;

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
    idleService.addIdleObserver(this, pageViewIdleTime);
  }

  /**
   * Uninitializes, removes any observers that need cleaning up manually.
   */
  uninit() {
    idleService.removeIdleObserver(this, pageViewIdleTime);
  }

  /**
   * Resets any stored user or interaction state.
   * Used by tests.
   */
  reset() {
    logConsole.debug("Reset");
    this.#interactions = new WeakMap();
    this.#userIsIdle = false;
    this._pageViewStartTime = Cu.now();
    this.#typingInteraction?.resetTypingInteraction();
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

    logConsole.debug("New interaction", docInfo);
    interaction = {
      url: docInfo.url,
      totalViewTime: 0,
      typingTime: 0,
      keypresses: 0,
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
    // Not having a browser passed to us probably means the tab has gone away
    // before we received the notification - due to the tab being a background
    // tab. Since that will be a non-active tab, it is acceptable that we don't
    // update the interaction. When switching away from active tabs, a TabSelect
    // notification is generated which we handle elsewhere.
    if (!browser) {
      return;
    }
    logConsole.debug("End of interaction");

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
      logConsole.debug("No update due to no active window");
      return;
    }

    // We do not update the interaction when the user is idle, since we will
    // have already updated it when idle was signalled.
    // Sometimes an interaction may be signalled before idle is cleared, however
    // worst case we'd only loose approx 2 seconds of interaction detail.
    if (this.#userIsIdle) {
      logConsole.debug("No update due to user is idle");
      return;
    }

    if (!browser) {
      browser = this.#activeWindow.gBrowser.selectedTab.linkedBrowser;
    }

    let interaction = this.#interactions.get(browser);
    if (!interaction) {
      logConsole.debug("No interaction to update");
      return;
    }

    interaction.totalViewTime += Cu.now() - this._pageViewStartTime;
    this._pageViewStartTime = Cu.now();

    const typingInteraction = this.#typingInteraction.getTypingInteraction();
    interaction.typingTime += typingInteraction.typingTime;
    interaction.keypresses += typingInteraction.keypresses;
    this.#typingInteraction.resetTypingInteraction();

    this._updateDatabase(interaction);
  }

  /**
   * Handles a window becoming active.
   *
   * @param {DOMWindow} win
   */
  #onActivateWindow(win) {
    logConsole.debug("Activate window");

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
    logConsole.debug("Deactivate window");

    this.#updateInteraction();
    this.#activeWindow = undefined;
  }

  /**
   * Get the timeout duration used for end-of-typing detection.
   *
   * @returns {number} timeout, in in milliseconds
   */
  _getTypingTimeout() {
    return TypingInteraction._TYPING_TIMEOUT;
  }

  /**
   * Set the timeout duration used for end-of-typing detection.
   * Provided to for test usage.
   *
   * @param {number} timeout
   *   Delay in milliseconds after a keypress after which end of typing is determined
   */
  _setTypingTimeout(timeout) {
    TypingInteraction._TYPING_TIMEOUT = timeout;
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
    logConsole.debug("Tab switch notified");

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
      case "idle":
        logConsole.debug("idle");
        // We save the state of the current interaction when we are notified
        // that the user is idle.
        this.#updateInteraction();
        this.#userIsIdle = true;
        break;
      case "active":
        logConsole.debug("active");
        this.#userIsIdle = false;
        this._pageViewStartTime = Cu.now();
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
    this.#typingInteraction.registerWindow(win);
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
    this.#typingInteraction.unregisterWindow(win);
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
  async _updateDatabase(interactionInfo) {
    logConsole.debug("Would update database: ", interactionInfo);
  }
}

const Interactions = new _Interactions();
