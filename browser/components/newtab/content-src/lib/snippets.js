const DATABASE_NAME = "snippets_db";
const DATABASE_VERSION = 1;
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
export const SNIPPETS_UPDATE_INTERVAL_MS = 14400000; // 4 hours.

const SNIPPETS_ENABLED_EVENT = "Snippets:Enabled";
const SNIPPETS_DISABLED_EVENT = "Snippets:Disabled";

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {ASRouterContent} from "content-src/asrouter/asrouter-content";

/**
 * SnippetsMap - A utility for cacheing values related to the snippet. It has
 *               the same interface as a Map, but is optionally backed by
 *               indexedDB for persistent storage.
 *               Call .connect() to open a database connection and restore any
 *               previously cached data, if necessary.
 *
 */
export class SnippetsMap extends Map {
  constructor(dispatch) {
    super();
    this._db = null;
    this._dispatch = dispatch;
  }

  set(key, value) {
    super.set(key, value);
    return this._dbTransaction(db => db.put(value, key));
  }

  delete(key) {
    super.delete(key);
    return this._dbTransaction(db => db.delete(key));
  }

  clear() {
    super.clear();
    this._dispatch(ac.OnlyToMain({type: at.SNIPPETS_BLOCKLIST_CLEARED}));
    return this._dbTransaction(db => db.clear());
  }

  get blockList() {
    return this.get("blockList") || [];
  }

  /**
   * blockSnippetById - Blocks a snippet given an id
   *
   * @param  {str|int} id   The id of the snippet
   * @return {Promise}      Resolves when the id has been written to indexedDB,
   *                        or immediately if the snippetMap is not connected
   */
  async blockSnippetById(id) {
    if (!id) {
      return;
    }
    const {blockList} = this;
    if (!blockList.includes(id)) {
      blockList.push(id);
      this._dispatch(ac.AlsoToMain({type: at.SNIPPETS_BLOCKLIST_UPDATED, data: id}));
      await this.set("blockList", blockList);
    }
  }

  disableOnboarding() {
    this._dispatch(ac.AlsoToMain({type: at.DISABLE_ONBOARDING}));
  }

  showFirefoxAccounts() {
    this._dispatch(ac.AlsoToMain({type: at.SHOW_FIREFOX_ACCOUNTS}));
  }

  getTotalBookmarksCount() {
    return new Promise(resolve => {
      this._dispatch(ac.OnlyToMain({type: at.TOTAL_BOOKMARKS_REQUEST}));
      global.RPMAddMessageListener("ActivityStream:MainToContent", function onMessage({data: action}) {
        if (action.type === at.TOTAL_BOOKMARKS_RESPONSE) {
          resolve(action.data);
          global.RPMRemoveMessageListener("ActivityStream:MainToContent", onMessage);
        }
      });
    });
  }

  getAddonsInfo() {
    return new Promise(resolve => {
      this._dispatch(ac.OnlyToMain({type: at.ADDONS_INFO_REQUEST}));
      global.RPMAddMessageListener("ActivityStream:MainToContent", function onMessage({data: action}) {
        if (action.type === at.ADDONS_INFO_RESPONSE) {
          resolve(action.data);
          global.RPMRemoveMessageListener("ActivityStream:MainToContent", onMessage);
        }
      });
    });
  }

  /**
   * connect - Attaches an indexedDB back-end to the Map so that any set values
   *           are also cached in a store. It also restores any existing values
   *           that are already stored in the indexedDB store.
   *
   * @return {type}  description
   */
  async connect() {
    // Open the connection
    const db = await this._openDB();

    // Restore any existing values
    await this._restoreFromDb(db);

    // Attach a reference to the db
    this._db = db;
  }

  /**
   * _dbTransaction - Returns a db transaction wrapped with the given modifier
   *                  function as a Promise. If the db has not been connected,
   *                  it resolves immediately.
   *
   * @param  {func} modifier A function to call with the transaction
   * @return {obj}           A Promise that resolves when the transaction has
   *                         completed or errored
   */
  _dbTransaction(modifier) {
    if (!this._db) {
      return Promise.resolve();
    }
    return new Promise((resolve, reject) => {
      const transaction = modifier(
        this._db
          .transaction(SNIPPETS_OBJECTSTORE_NAME, "readwrite")
          .objectStore(SNIPPETS_OBJECTSTORE_NAME)
      );
      transaction.onsuccess = event => resolve();

      /* istanbul ignore next */
      transaction.onerror = event => reject(transaction.error);
    });
  }

  _openDB() {
    return new Promise((resolve, reject) => {
      const openRequest = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);

      /* istanbul ignore next */
      openRequest.onerror = event => {
        // Try to delete the old database so that we can start this process over
        // next time.
        indexedDB.deleteDatabase(DATABASE_NAME);
        reject(event);
      };

      openRequest.onupgradeneeded = event => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains(SNIPPETS_OBJECTSTORE_NAME)) {
          db.createObjectStore(SNIPPETS_OBJECTSTORE_NAME);
        }
      };

      openRequest.onsuccess = event => {
        let db = event.target.result;

        /* istanbul ignore next */
        db.onerror = err => console.error(err); // eslint-disable-line no-console
        /* istanbul ignore next */
        db.onversionchange = versionChangeEvent => versionChangeEvent.target.close();

        resolve(db);
      };
    });
  }

  _restoreFromDb(db) {
    return new Promise((resolve, reject) => {
      let cursorRequest;
      try {
        cursorRequest = db.transaction(SNIPPETS_OBJECTSTORE_NAME)
          .objectStore(SNIPPETS_OBJECTSTORE_NAME).openCursor();
      } catch (err) {
        // istanbul ignore next
        reject(err);
        // istanbul ignore next
        return;
      }

      /* istanbul ignore next */
      cursorRequest.onerror = event => reject(event);

      cursorRequest.onsuccess = event => {
        let cursor = event.target.result;
        // Populate the cache from the persistent storage.
        if (cursor) {
          if (cursor.value !== "blockList") {
            this.set(cursor.key, cursor.value);
          }
          cursor.continue();
        } else {
          // We are done.
          resolve();
        }
      };
    });
  }
}

/**
 * SnippetsProvider - Initializes a SnippetsMap and loads snippets from a
 *                    remote location, or else default snippets if the remote
 *                    snippets cannot be retrieved.
 */
export class SnippetsProvider {
  constructor(dispatch) {
    // Initialize the Snippets Map and attaches it to a global so that
    // the snippet payload can interact with it.
    global.gSnippetsMap = new SnippetsMap(dispatch);
    this._onAction = this._onAction.bind(this);
  }

  get snippetsMap() {
    return global.gSnippetsMap;
  }

  async _refreshSnippets() {
    // Check if the cached version of of the snippets in snippetsMap. If it's too
    // old, blow away the entire snippetsMap.
    const cachedVersion = this.snippetsMap.get("snippets-cached-version");

    if (cachedVersion !== this.appData.version) {
      this.snippetsMap.clear();
    }

    // Has enough time passed for us to require an update?
    const lastUpdate = this.snippetsMap.get("snippets-last-update");
    const needsUpdate = !(lastUpdate >= 0) || Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS;

    if (needsUpdate && this.appData.snippetsURL) {
      this.snippetsMap.set("snippets-last-update", Date.now());
      try {
        const response = await fetch(this.appData.snippetsURL);
        if (response.status === 200) {
          const payload = await response.text();

          this.snippetsMap.set("snippets", payload);
          this.snippetsMap.set("snippets-cached-version", this.appData.version);
        }
      } catch (e) {
        console.error(e); // eslint-disable-line no-console
      }
    }
  }

  _noSnippetFallback() {
    // TODO
  }

  _forceOnboardingVisibility(shouldBeVisible) {
    const onboardingEl = document.getElementById("onboarding-notification-bar");

    if (onboardingEl) {
      onboardingEl.style.display = shouldBeVisible ? "" : "none";
    }
  }

  _showRemoteSnippets() {
    const snippetsEl = document.getElementById(this.elementId);
    const payload = this.snippetsMap.get("snippets");

    if (!snippetsEl) {
      throw new Error(`No element was found with id '${this.elementId}'.`);
    }

    // This could happen if fetching failed
    if (!payload) {
      throw new Error("No remote snippets were found in gSnippetsMap.");
    }

    if (typeof payload !== "string") {
      throw new Error("Snippet payload was incorrectly formatted");
    }

    // Note that injecting snippets can throw if they're invalid XML.
    // eslint-disable-next-line no-unsanitized/property
    snippetsEl.innerHTML = payload;

    // Scripts injected by innerHTML are inactive, so we have to relocate them
    // through DOM manipulation to activate their contents.
    for (const scriptEl of snippetsEl.getElementsByTagName("script")) {
      const relocatedScript = document.createElement("script");
      relocatedScript.text = scriptEl.text;
      scriptEl.parentNode.replaceChild(relocatedScript, scriptEl);
    }
  }

  _onAction(msg) {
    if (msg.data.type === at.SNIPPET_BLOCKED) {
      if (!this.snippetsMap.blockList.includes(msg.data.data)) {
        this.snippetsMap.set("blockList", this.snippetsMap.blockList.concat(msg.data.data));
        document.getElementById("snippets-container").style.display = "none";
      }
    }
  }

  /**
   * init - Fetch the snippet payload and show snippets
   *
   * @param  {obj} options
   * @param  {str} options.appData.snippetsURL  The URL from which we fetch snippets
   * @param  {int} options.appData.version  The current snippets version
   * @param  {str} options.elementId  The id of the element in which to inject snippets
   * @param  {bool} options.connect  Should gSnippetsMap connect to indexedDB?
   */
  async init(options) {
    Object.assign(this, {
      appData: {},
      elementId: "snippets",
      connect: true
    }, options);

    // Add listener so we know when snippets are blocked on other pages
    if (global.RPMAddMessageListener) {
      global.RPMAddMessageListener("ActivityStream:MainToContent", this._onAction);
    }

    // TODO: Requires enabling indexedDB on newtab
    // Restore the snippets map from indexedDB
    if (this.connect) {
      try {
        await this.snippetsMap.connect();
      } catch (e) {
        console.error(e); // eslint-disable-line no-console
      }
    }

    // Cache app data values so they can be accessible from gSnippetsMap
    for (const key of Object.keys(this.appData)) {
      if (key === "blockList") {
        this.snippetsMap.set("blockList", this.appData[key]);
      } else {
        this.snippetsMap.set(`appData.${key}`, this.appData[key]);
      }
    }

    // Refresh snippets, if enough time has passed.
    await this._refreshSnippets();

    // Try showing remote snippets, falling back to defaults if necessary.
    try {
      this._showRemoteSnippets();
    } catch (e) {
      this._noSnippetFallback(e);
    }

    window.dispatchEvent(new Event(SNIPPETS_ENABLED_EVENT));

    this._forceOnboardingVisibility(true);
    this.initialized = true;
  }

  uninit() {
    window.dispatchEvent(new Event(SNIPPETS_DISABLED_EVENT));
    this._forceOnboardingVisibility(false);
    if (global.RPMRemoveMessageListener) {
      global.RPMRemoveMessageListener("ActivityStream:MainToContent", this._onAction);
    }
    this.initialized = false;
  }
}

/**
 * addSnippetsSubscriber - Creates a SnippetsProvider that Initializes
 *                         when the store has received the appropriate
 *                         Snippet data.
 *
 * @param  {obj} store   The redux store
 * @return {obj}         Returns the snippets instance, asrouterContent instance and unsubscribe function
 */
export function addSnippetsSubscriber(store) {
  const snippets = new SnippetsProvider(store.dispatch);
  const asrouterContent = new ASRouterContent();

  let initializing = false;

  store.subscribe(async () => {
    const state = store.getState();
    const isASRouterEnabled = state.Prefs.values.asrouterExperimentEnabled && state.Prefs.values.asrouterOnboardingCohort > 0;
    // state.Prefs.values["feeds.snippets"]:  Should snippets be shown?
    // state.Snippets.initialized             Is the snippets data initialized?
    // snippets.initialized:                  Is SnippetsProvider currently initialised?
    if (state.Prefs.values["feeds.snippets"] &&
      // If the message center experiment is enabled, don't show snippets
      !isASRouterEnabled &&
      !state.Prefs.values.disableSnippets &&
      state.Snippets.initialized &&
      !snippets.initialized &&
      // Don't call init multiple times
      !initializing &&
      location.href !== "about:welcome"
    ) {
      initializing = true;
      await snippets.init({appData: state.Snippets});
      initializing = false;
    } else if (
      (state.Prefs.values["feeds.snippets"] === false ||
        state.Prefs.values.disableSnippets === true) &&
      snippets.initialized
    ) {
      snippets.uninit();
    }

    // Turn on AS Router snippets if the experiment is enabled and the snippets pref is on;
    // otherwise, turn it off.
    if (
      (state.Prefs.values.asrouterExperimentEnabled || state.Prefs.values.asrouterOnboardingCohort > 0) &&
      state.Prefs.values["feeds.snippets"] &&
      !asrouterContent.initialized) {
      asrouterContent.init();
    } else if (
      ((!state.Prefs.values.asrouterExperimentEnabled && state.Prefs.values.asrouterOnboardingCohort === 0) || !state.Prefs.values["feeds.snippets"]) &&
      asrouterContent.initialized
    ) {
      asrouterContent.uninit();
    }
  });

  // These values are returned for testing purposes
  return {snippets, asrouterContent};
}
