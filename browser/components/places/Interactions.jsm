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
  clearTimeout: "resource://gre/modules/Timer.jsm",
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
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

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "saveInterval",
  "browser.places.interactions.saveInterval",
  10000
);

const DOMWINDOW_OPENED_TOPIC = "domwindowopened";

/**
 * Returns a monotonically increasing timestamp, that is critical to distinguish
 * database entries by creation time.
 */
let gLastTime = 0;
function monotonicNow() {
  let time = Date.now();
  if (time == gLastTime) {
    time++;
  }
  return (gLastTime = time);
}

/**
 * The TypingInteraction object measures time spent typing on the current interaction.
 * This is consists of the current typing metrics as well as accumulated typing metrics.
 */
class TypingInteraction {
  /**
   * Returns an object with all current and accumulated typing metrics.
   *
   * @returns {object} with properties typingTime, keypresses
   */
  getTypingInteraction() {
    let typingInteraction = { typingTime: 0, keypresses: 0 };
    const interactionData = ChromeUtils.consumeInteractionData();
    const typing = interactionData.Typing;
    if (typing) {
      typingInteraction.typingTime += typing.interactionTimeInMilliseconds;
      typingInteraction.keypresses += typing.interactionCount;
    }
    return typingInteraction;
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
 * @property {number} scrollingTime
 *   Time in milliseconds that the user spent scrolling the page
 * @property {number} scrollingDistance
 *   The distance, in pixels, that the user scrolled the page
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
   * Stores interactions in the database, see the InteractionsStore class.
   * This is created lazily, see the `store` getter.
   */
  #store = undefined;

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
  async reset() {
    logConsole.debug("Reset");
    this.#interactions = new WeakMap();
    this.#userIsIdle = false;
    this._pageViewStartTime = Cu.now();
    ChromeUtils.consumeInteractionData();
    await this.store.reset();
  }

  /**
   * Retrieve the underlying InteractionsStore object. This exists for testing
   * purposes and should not be abused by production code (for example it'd be
   * a bad idea to force flushes).
   */
  get store() {
    if (!this.#store) {
      this.#store = new InteractionsStore();
    }
    return this.#store;
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

    if (InteractionsBlocklist.isUrlBlocklisted(docInfo.url)) {
      logConsole.debug("URL is blocklisted", docInfo);
      return;
    }

    logConsole.debug("New interaction", docInfo);
    let now = monotonicNow();
    interaction = {
      url: docInfo.url,
      totalViewTime: 0,
      typingTime: 0,
      keypresses: 0,
      scrollingTime: 0,
      scrollingDistance: 0,
      created_at: now,
      updated_at: now,
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
    interaction.updated_at = monotonicNow();

    logConsole.debug("Add to store: ", interaction);
    this.store.add(interaction);
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
}

const Interactions = new _Interactions();

/**
 * Store interactions data in the Places database.
 * To improve performance the writes are buffered every `saveInterval`
 * milliseconds. Even if this means we could be trying to write interaction for
 * pages that in the meanwhile have been removed, that's not a problem because
 * we won't be able to insert entries having a NULL place_id, they will just be
 * ignored.
 * Use .add(interaction) to request storing of an interaction.
 * Use .pendingPromise to await for any pending writes to have happened.
 */
class InteractionsStore {
  /**
   * Timer to run database updates on.
   */
  #timer = undefined;
  /**
   * Tracks interactions replicating the unique index in the underlying schema.
   * Interactions are keyed by url and then created_at.
   * @type {Map<string, Map<number, InteractionInfo>>}
   */
  #interactions = new Map();
  /**
   * Used to unblock the queue of promises when the timer is cleared.
   */
  #timerResolve = undefined;

  constructor() {
    // Block async shutdown to ensure the last write goes through.
    this.progress = {};
    PlacesUtils.history.shutdownClient.jsclient.addBlocker(
      "Interactions.jsm:: store",
      async () => this.flush(),
      { fetchState: () => this.progress }
    );

    // Can be used to wait for the last pending write to have happened.
    this.pendingPromise = Promise.resolve();
  }

  /**
   * Synchronizes the pending interactions with the storage device.
   * @returns {Promise} resolved when the pending data is on disk.
   */
  async flush() {
    if (this.#timer) {
      clearTimeout(this.#timer);
      this.#timerResolve();
      await this.#updateDatabase();
    }
  }

  /**
   * Completely clears the store and any pending writes.
   * This exists for testing purposes.
   */
  async reset() {
    await PlacesUtils.withConnectionWrapper(
      "Interactions.jsm::reset",
      async db => {
        await db.executeCached(`DELETE FROM moz_places_metadata`);
      }
    );
    if (this.#timer) {
      clearTimeout(this.#timer);
      this.#timerResolve();
      this.#interactions.clear();
    }
  }

  /**
   * Registers an interaction to be stored persistently. At the end of the call
   * the interaction has not yet been added to the store, tests can await
   * flushStore() for that.
   *
   * @param {InteractionInfo} interaction
   *   The document information to write.
   */
  add(interaction) {
    let interactionsForUrl = this.#interactions.get(interaction.url);
    if (!interactionsForUrl) {
      interactionsForUrl = new Map();
      this.#interactions.set(interaction.url, interactionsForUrl);
    }
    interactionsForUrl.set(interaction.created_at, interaction);

    if (!this.#timer) {
      let promise = new Promise(resolve => {
        this.#timerResolve = resolve;
        this.#timer = setTimeout(() => {
          logConsole.debug("Save Timer");
          this.#updateDatabase()
            .catch(Cu.reportError)
            .then(resolve);
        }, saveInterval);
      });
      this.pendingPromise = this.pendingPromise.then(() => promise);
    }
  }

  async #updateDatabase() {
    this.#timer = undefined;

    // Reset the buffer.
    let interactions = this.#interactions;
    // Don't clear() this, since that would also clear interactions.
    this.#interactions = new Map();

    let params = {};
    let SQLInsertFragments = [];
    let i = 0;
    for (let interactionsForUrl of interactions.values()) {
      for (let interaction of interactionsForUrl.values()) {
        params[`url${i}`] = interaction.url;
        params[`created_at${i}`] = interaction.created_at;
        params[`updated_at${i}`] = interaction.updated_at;
        params[`total_view_time${i}`] =
          Math.round(interaction.totalViewTime) || 0;
        params[`typing_time${i}`] = Math.round(interaction.typingTime) || 0;
        params[`key_presses${i}`] = interaction.keypresses || 0;
        params[`scrolling_time${i}`] =
          Math.round(interaction.scrollingTime) || 0;
        params[`scrolling_distance${i}`] =
          Math.round(interaction.scrollingDistance) || 0;
        SQLInsertFragments.push(`(
          (SELECT id FROM moz_places_metadata
            WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url${i}) AND url = :url${i})
              AND created_at = :created_at${i}),
          (SELECT id FROM moz_places WHERE url_hash = hash(:url${i}) AND url = :url${i}),
          :created_at${i},
          :updated_at${i},
          :total_view_time${i},
          :typing_time${i},
          :key_presses${i},
          :scrolling_time${i},
          :scrolling_distance${i}
        )`);
        i++;
      }
    }
    logConsole.debug(`Storing ${i} entries in the database`);
    this.progress.pendingUpdates = i;
    await PlacesUtils.withConnectionWrapper(
      "Interactions.jsm::updateDatabase",
      async db => {
        await db.executeCached(
          `
          WITH inserts (id, place_id, created_at, updated_at, total_view_time, typing_time, key_presses, scrolling_time, scrolling_distance) AS (
            VALUES ${SQLInsertFragments.join(", ")}
          )
          INSERT OR REPLACE INTO moz_places_metadata (
            id, place_id, created_at, updated_at, total_view_time, typing_time, key_presses, scrolling_time, scrolling_distance
          ) SELECT * FROM inserts WHERE place_id NOT NULL;
        `,
          params
        );
      }
    );
    this.progress.pendingUpdates = 0;
  }
}
