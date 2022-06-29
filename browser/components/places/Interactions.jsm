/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Interactions"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
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

XPCOMUtils.defineLazyServiceGetters(lazy, {
  idleService: ["@mozilla.org/widget/useridleservice;1", "nsIUserIdleService"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "pageViewIdleTime",
  "browser.places.interactions.pageViewIdleTime",
  60
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshotIdleTime",
  "browser.places.interactions.snapshotIdleTime",
  2
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
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
 * @property {Interactions.DOCUMENT_TYPE} documentType
 *   The type of the document.
 * @property {number} typingTime
 *   Time in milliseconds that the user typed on the page
 * @property {number} keypresses
 *   The number of keypresses made on the page
 * @property {number} scrollingTime
 *   Time in milliseconds that the user spent scrolling the page
 * @property {number} scrollingDistance
 *   The distance, in pixels, that the user scrolled the page
 * @property {number} created_at
 *   Creation time as the number of milliseconds since the epoch.
 * @property {number} updated_at
 *   Last updated time as the number of milliseconds since the epoch.
 * @property {string} referrer
 *   The referrer to the url of the page that was interacted with (may be empty)
 *
 */

/**
 * The Interactions object sets up listeners and other approriate tools for
 * obtaining interaction information and passing it to the InteractionsManager.
 */
class _Interactions {
  DOCUMENT_TYPE = {
    // Used when the document type is unknown.
    GENERIC: 0,
    // Used for pages serving media, e.g. videos.
    MEDIA: 1,
  };

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
   * Whether the component has been initialized.
   */
  #initialized = false;

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
        events: {
          DOMContentLoaded: {},
          pagehide: { mozSystemGroup: true },
        },
      },
      messageManagerGroups: ["browsers"],
    });

    this.#activeWindow = Services.wm.getMostRecentBrowserWindow();

    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      if (!win.closed) {
        this.#registerWindow(win);
      }
    }
    Services.obs.addObserver(this, DOMWINDOW_OPENED_TOPIC, true);
    Services.obs.addObserver(this, "places-snapshots-added", true);
    lazy.idleService.addIdleObserver(this, lazy.pageViewIdleTime);
    this.#initialized = true;
  }

  /**
   * Uninitializes, removes any observers that need cleaning up manually.
   */
  uninit() {
    if (this.#initialized) {
      lazy.idleService.removeIdleObserver(this, lazy.pageViewIdleTime);
    }
  }

  /**
   * Resets any stored user or interaction state.
   * Used by tests.
   */
  async reset() {
    lazy.logConsole.debug("Database reset");
    this.#interactions = new WeakMap();
    this.#userIsIdle = false;
    this._pageViewStartTime = Cu.now();
    ChromeUtils.consumeInteractionData();
    await _Interactions.interactionUpdatePromise;
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

    if (lazy.InteractionsBlocklist.isUrlBlocklisted(docInfo.url)) {
      lazy.logConsole.debug(
        "Ignoring a page as the URL is blocklisted",
        docInfo
      );
      return;
    }

    lazy.logConsole.debug("Tracking a new interaction", docInfo);
    let now = monotonicNow();
    interaction = {
      url: docInfo.url,
      referrer: docInfo.referrer,
      totalViewTime: 0,
      typingTime: 0,
      keypresses: 0,
      scrollingTime: 0,
      scrollingDistance: 0,
      created_at: now,
      updated_at: now,
    };
    this.#interactions.set(browser, interaction);

    // Lock any potential page data in memory until we've decided if this page
    // needs to become a snapshot or not.
    lazy.PageDataService.lockEntry(this, docInfo.url);

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
    lazy.logConsole.debug("Saw the end of an interaction");

    this.#updateInteraction(browser);
    this.#interactions.delete(browser);
  }

  /**
   * Updates the current interaction
   *
   * @param {Browser} [browser]
   *   The browser object that has triggered the update, if known. This is
   *   used to check if the browser is in the active window, and as an
   *   optimization to avoid obtaining the browser object.
   */
  #updateInteraction(browser = undefined) {
    _Interactions.#updateInteraction_async(
      browser,
      this.#activeWindow,
      this.#userIsIdle,
      this.#interactions,
      this._pageViewStartTime,
      this.store
    );
  }

  /**
   * Stores the promise created in updateInteraction_async so that we can await its fulfillment
   * when sychronization is needed.
   */
  static interactionUpdatePromise = Promise.resolve();

  /**
   * Returns the interactions update promise to be used when sychronization is needed from tests.
   */
  get interactionUpdatePromise() {
    return _Interactions.interactionUpdatePromise;
  }

  /**
   * Updates the current interaction on fulfillment of the asynchronous collection of scrolling interactions.
   *
   *  @param {Browser} browser
   *  @param {DOMWindow} activeWindow
   *  @param {boolean} userIsIdle
   *  @param {WeakMap<browser, InteractionInfo>} interactions
   *  @param {number} pageViewStartTime
   *  @param {InteractionsStore} store
   */
  static async #updateInteraction_async(
    browser,
    activeWindow,
    userIsIdle,
    interactions,
    pageViewStartTime,
    store
  ) {
    if (!activeWindow || (browser && browser.ownerGlobal != activeWindow)) {
      lazy.logConsole.debug(
        "Not updating interaction as there is no active window"
      );
      return;
    }

    // We do not update the interaction when the user is idle, since we will
    // have already updated it when idle was signalled.
    // Sometimes an interaction may be signalled before idle is cleared, however
    // worst case we'd only loose approx 2 seconds of interaction detail.
    if (userIsIdle) {
      lazy.logConsole.debug("Not updating interaction as the user is idle");
      return;
    }

    if (!browser) {
      browser = activeWindow.gBrowser.selectedTab.linkedBrowser;
    }

    let interaction = interactions.get(browser);
    if (!interaction) {
      lazy.logConsole.debug("No interaction to update");
      return;
    }

    interaction.totalViewTime += Cu.now() - pageViewStartTime;
    Interactions._pageViewStartTime = Cu.now();

    const interactionData = ChromeUtils.consumeInteractionData();
    const typing = interactionData.Typing;
    if (typing) {
      interaction.typingTime += typing.interactionTimeInMilliseconds;
      interaction.keypresses += typing.interactionCount;
    }

    // Collect the scrolling data and add the interaction to the store on completion
    _Interactions.interactionUpdatePromise = _Interactions.interactionUpdatePromise
      .then(async () => ChromeUtils.collectScrollingData())
      .then(
        result => {
          interaction.scrollingTime += result.interactionTimeInMilliseconds;
          interaction.scrollingDistance += result.scrollingDistanceInPixels;
        },
        reason => {
          Cu.reportError(reason);
        }
      )
      .then(() => {
        interaction.updated_at = monotonicNow();

        lazy.logConsole.debug("Add to store: ", interaction);
        store.add(interaction);
      });
  }

  /**
   * Handles a window becoming active.
   *
   * @param {DOMWindow} win
   */
  #onActivateWindow(win) {
    lazy.logConsole.debug("Window activated");

    if (lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
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
    lazy.logConsole.debug("Window deactivate");

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
    lazy.logConsole.debug("Tab switched");

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
      case "places-snapshots-added":
        // For manually created snapshots we want to flush interactions to disk
        // asap, so that heuristics can use them.
        data = JSON.parse(data);
        if (
          data.some(d => d.userPersisted != lazy.Snapshots.USER_PERSISTED.NO)
        ) {
          lazy.logConsole.debug("User added some snapshots");
          this.#updateInteraction();
        }
        break;
      case "idle":
        lazy.logConsole.debug("User went idle");
        // We save the state of the current interaction when we are notified
        // that the user is idle.
        this.#updateInteraction();
        this.#userIsIdle = true;
        break;
      case "active":
        lazy.logConsole.debug("User became active");
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
    if (lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
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
  /*
   * A list of URLs that have had interactions updated since we last checked for
   * new snapshots.
   * @type {Set<string>}
   */
  #potentialSnapshots = new Set();
  /*
   * Whether the user has been idle for more than the value of
   * `browser.places.interactions.snapshotIdleTime`.
   */
  #userIsIdle = false;

  constructor() {
    // Block async shutdown to ensure the last write goes through.
    this.progress = {};
    lazy.PlacesUtils.history.shutdownClient.jsclient.addBlocker(
      "Interactions.jsm:: store",
      async () => this.flush(),
      { fetchState: () => this.progress }
    );

    // Can be used to wait for the last pending write to have happened.
    this.pendingPromise = Promise.resolve();

    lazy.idleService.addIdleObserver(this, lazy.snapshotIdleTime);
  }

  /**
   * Tells the snapshot service to check all of the potential snapshots.
   *
   * @returns {Promise}
   */
  async updateSnapshots() {
    let urls = [...this.#potentialSnapshots];
    this.#potentialSnapshots.clear();
    await lazy.Snapshots.updateSnapshots(urls);

    for (let url of urls) {
      lazy.PageDataService.unlockEntry(Interactions, url);
    }
  }

  /**
   * Synchronizes the pending interactions with the storage device.
   * @returns {Promise} resolved when the pending data is on disk.
   */
  async flush() {
    if (this.#timer) {
      lazy.clearTimeout(this.#timer);
      this.#timerResolve();
      await this.#updateDatabase();
      await this.updateSnapshots();
    }
  }

  /**
   * Completely clears the store and any pending writes.
   * This exists for testing purposes.
   */
  async reset() {
    await lazy.PlacesUtils.withConnectionWrapper(
      "Interactions.jsm::reset",
      async db => {
        await db.executeCached(`DELETE FROM moz_places_metadata`);
      }
    );
    if (this.#timer) {
      lazy.clearTimeout(this.#timer);
      this.#timer = undefined;
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
    lazy.logConsole.debug("Preparing interaction for storage", interaction);

    let interactionsForUrl = this.#interactions.get(interaction.url);
    if (!interactionsForUrl) {
      interactionsForUrl = new Map();
      this.#interactions.set(interaction.url, interactionsForUrl);
    }
    interactionsForUrl.set(interaction.created_at, interaction);

    if (!this.#timer) {
      let promise = new Promise(resolve => {
        this.#timerResolve = resolve;
        this.#timer = lazy.setTimeout(() => {
          this.#updateDatabase()
            .catch(Cu.reportError)
            .then(resolve);
        }, lazy.saveInterval);
      });
      this.pendingPromise = this.pendingPromise.then(() => promise);
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
      case "idle":
        this.#userIsIdle = true;
        this.updateSnapshots();
        break;
      case "active":
        this.#userIsIdle = false;
    }
  }

  async #updateDatabase() {
    this.#timer = undefined;

    // Reset the buffer.
    let interactions = this.#interactions;
    if (!interactions.size) {
      return;
    }
    // Don't clear() this, since that would also clear interactions.
    this.#interactions = new Map();

    let params = {};
    let SQLInsertFragments = [];
    let i = 0;
    for (let [url, interactionsForUrl] of interactions) {
      this.#potentialSnapshots.add(url);

      for (let interaction of interactionsForUrl.values()) {
        params[`url${i}`] = interaction.url;
        params[`referrer${i}`] = interaction.referrer;
        params[`created_at${i}`] = interaction.created_at;
        params[`updated_at${i}`] = interaction.updated_at;
        params[`document_type${i}`] =
          interaction.documentType ?? Interactions.DOCUMENT_TYPE.GENERIC;
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
          (SELECT id FROM moz_places WHERE url_hash = hash(:referrer${i}) AND url = :referrer${i} AND :referrer${i} != :url${i}),
          :created_at${i},
          :updated_at${i},
          :document_type${i},
          :total_view_time${i},
          :typing_time${i},
          :key_presses${i},
          :scrolling_time${i},
          :scrolling_distance${i}
        )`);
        i++;
      }
    }

    lazy.logConsole.debug(`Storing ${i} entries in the database`);

    this.progress.pendingUpdates = i;
    await lazy.PlacesUtils.withConnectionWrapper(
      "Interactions.jsm::updateDatabase",
      async db => {
        await db.executeCached(
          `
          WITH inserts (id, place_id, referrer_place_id, created_at, updated_at, document_type, total_view_time, typing_time, key_presses, scrolling_time, scrolling_distance) AS (
            VALUES ${SQLInsertFragments.join(", ")}
          )
          INSERT OR REPLACE INTO moz_places_metadata (
            id, place_id, referrer_place_id, created_at, updated_at, document_type, total_view_time, typing_time, key_presses, scrolling_time, scrolling_distance
          ) SELECT * FROM inserts WHERE place_id NOT NULL;
        `,
          params
        );
      }
    );
    this.progress.pendingUpdates = 0;

    Services.obs.notifyObservers(null, "places-metadata-updated");

    if (this.#userIsIdle) {
      this.updateSnapshots();
    }
  }
}
