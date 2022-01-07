/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "KeywordTree",
  "ONBOARDING_CHOICE",
  "UrlbarQuickSuggest",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  QUICK_SUGGEST_SOURCE: "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["TextDecoder"]);

const log = console.createInstance({
  prefix: "QuickSuggest",
  maxLogLevel: UrlbarPrefs.get("quicksuggest.log") ? "All" : "Warn",
});

const RS_COLLECTION = "quicksuggest";

// Categories that should show "Firefox Suggest" instead of "Sponsored"
const NONSPONSORED_IAB_CATEGORIES = new Set(["5 - Education"]);

const FEATURE_AVAILABLE = "quickSuggestEnabled";
const SEEN_DIALOG_PREF = "quicksuggest.showedOnboardingDialog";
const RESTARTS_PREF = "quicksuggest.seenRestarts";
const DIALOG_VERSION_PREF = "quicksuggest.onboardingDialogVersion";
const DIALOG_VARIATION_PREF = "quickSuggestOnboardingDialogVariation";

// Values returned by the onboarding dialog depending on the user's response.
// These values are used in telemetry events, so be careful about changing them.
const ONBOARDING_CHOICE = {
  ACCEPT_2: "accept_2",
  CLOSE_1: "close_1",
  DISMISS_1: "dismiss_1",
  DISMISS_2: "dismiss_2",
  LEARN_MORE_2: "learn_more_2",
  NOT_NOW_2: "not_now_2",
  REJECT_2: "reject_2",
};

const ONBOARDING_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.html";

// This is a score in the range [0, 1] used by the provider to compare
// suggestions from remote settings to suggestions from Merino. Remote settings
// suggestions don't have a natural score so we hardcode a value, and we choose
// a low value to allow Merino to experiment with a broad range of scores.
const SUGGESTION_SCORE = 0.2;

/**
 * Fetches the suggestions data from RemoteSettings and builds the tree
 * to provide suggestions for UrlbarProviderQuickSuggest.
 */
class Suggestions {
  constructor() {
    UrlbarPrefs.addObserver(this);
    NimbusFeatures.urlbar.onUpdate(() => this._queueSettingsSetup());

    this._queueSettingsTask(() => {
      return new Promise(resolve => {
        Services.tm.idleDispatchToMainThread(() => {
          this._queueSettingsSetup();
          resolve();
        });
      });
    });
  }

  /**
   * @returns {number}
   *   A score in the range [0, 1] that can be used to compare suggestions from
   *   remote settings to suggestions from Merino. Remote settings suggestions
   *   don't have a natural score so we hardcode a value.
   */
  get SUGGESTION_SCORE() {
    return SUGGESTION_SCORE;
  }

  /**
   * @returns {Promise}
   *   Resolves when any ongoing updates to the suggestions data are done.
   */
  get readyPromise() {
    if (!this._settingsTaskQueue.length) {
      return Promise.resolve();
    }
    return new Promise(resolve => {
      this._emptySettingsTaskQueueCallbacks.push(resolve);
    });
  }

  /**
   * Handle queries from the Urlbar.
   *
   * @param {string} phrase
   *   The search string.
   */
  async query(phrase) {
    log.info("Handling query for", phrase);
    phrase = phrase.toLowerCase();
    let resultID = this._tree.get(phrase);
    if (resultID === null) {
      return null;
    }
    let result = this._results.get(resultID);
    if (!result) {
      return null;
    }
    return {
      full_keyword: this.getFullKeyword(phrase, result.keywords),
      title: result.title,
      url: result.url,
      click_url: result.click_url,
      impression_url: result.impression_url,
      block_id: result.id,
      advertiser: result.advertiser,
      is_sponsored: !NONSPONSORED_IAB_CATEGORIES.has(result.iab_category),
      score: SUGGESTION_SCORE,
      source: QUICK_SUGGEST_SOURCE.REMOTE_SETTINGS,
      icon: await this._fetchIcon(result.icon),
      position: result.position,
    };
  }

  /**
   * Gets the full keyword (i.e., suggestion) for a result and query.  The data
   * doesn't include full keywords, so we make our own based on the result's
   * keyword phrases and a particular query.  We use two heuristics:
   *
   * (1) Find the first keyword phrase that has more words than the query.  Use
   *     its first `queryWords.length` words as the full keyword.  e.g., if the
   *     query is "moz" and `result.keywords` is ["moz", "mozi", "mozil",
   *     "mozill", "mozilla", "mozilla firefox"], pick "mozilla firefox", pop
   *     off the "firefox" and use "mozilla" as the full keyword.
   * (2) If there isn't any keyword phrase with more words, then pick the
   *     longest phrase.  e.g., pick "mozilla" in the previous example (assuming
   *     the "mozilla firefox" phrase isn't there).  That might be the query
   *     itself.
   *
   * @param {string} query
   *   The query string that matched `result`.
   * @param {array} keywords
   *   An array of result keywords.
   * @returns {string}
   *   The full keyword.
   */
  getFullKeyword(query, keywords) {
    let longerPhrase;
    let trimmedQuery = query.trim();
    let queryWords = trimmedQuery.split(" ");

    for (let phrase of keywords) {
      if (phrase.startsWith(query)) {
        let trimmedPhrase = phrase.trim();
        let phraseWords = trimmedPhrase.split(" ");
        // As an exception to (1), if the query ends with a space, then look for
        // phrases with one more word so that the suggestion includes a word
        // following the space.
        let extra = query.endsWith(" ") ? 1 : 0;
        let len = queryWords.length + extra;
        if (len < phraseWords.length) {
          // We found a phrase with more words.
          return phraseWords.slice(0, len).join(" ");
        }
        if (
          query.length < phrase.length &&
          (!longerPhrase || longerPhrase.length < trimmedPhrase.length)
        ) {
          // We found a longer phrase with the same number of words.
          longerPhrase = trimmedPhrase;
        }
      }
    }
    return longerPhrase || trimmedQuery;
  }

  /**
   * An onboarding dialog can be shown to the users who are enrolled into
   * the QuickSuggest experiments or rollouts. This behavior is controlled
   * by the pref `browser.urlbar.quicksuggest.shouldShowOnboardingDialog`
   * which can be remotely configured by Nimbus.
   *
   * Given that the release may overlap with another onboarding dialog, we may
   * wait for a few restarts before showing the QuickSuggest dialog. This can
   * be remotely configured by Nimbus through
   * `quickSuggestShowOnboardingDialogAfterNRestarts`, the default is 0.
   *
   * @returns {boolean}
   *   True if the dialog was shown and false if not.
   */
  async maybeShowOnboardingDialog() {
    // The call to this method races scenario initialization on startup, and the
    // Nimbus variables we rely on below depend on the scenario, so wait for it
    // to be initialized.
    await UrlbarPrefs.firefoxSuggestScenarioStartupPromise;

    // If quicksuggest is not available, the onboarding dialog is configured to
    // be skipped, the user has already seen the dialog, or has otherwise opted
    // in already, then we won't show the quicksuggest onboarding.
    if (
      !UrlbarPrefs.get(FEATURE_AVAILABLE) ||
      !UrlbarPrefs.get("quickSuggestShouldShowOnboardingDialog") ||
      UrlbarPrefs.get(SEEN_DIALOG_PREF) ||
      UrlbarPrefs.get("quicksuggest.dataCollection.enabled")
    ) {
      return false;
    }

    // Wait a number of restarts before showing the dialog.
    let restartsSeen = UrlbarPrefs.get(RESTARTS_PREF);
    if (
      restartsSeen <
      UrlbarPrefs.get("quickSuggestShowOnboardingDialogAfterNRestarts")
    ) {
      UrlbarPrefs.set(RESTARTS_PREF, restartsSeen + 1);
      return false;
    }

    let win = BrowserWindowTracker.getTopWindow();

    let variationType;
    try {
      // An error happens if the pref is not in user prefs.
      variationType = UrlbarPrefs.get(DIALOG_VARIATION_PREF).toLowerCase();
    } catch (e) {}

    let params = { choice: undefined, variationType, visitedMain: false };
    await win.gDialogBox.open(ONBOARDING_URI, params);

    UrlbarPrefs.set(SEEN_DIALOG_PREF, true);
    UrlbarPrefs.set(
      DIALOG_VERSION_PREF,
      JSON.stringify({ version: 1, variation: variationType })
    );

    // Record the user's opt-in choice on the user branch. This pref is sticky,
    // so it will retain its user-branch value regardless of what the particular
    // default was at the time.
    let optedIn = params.choice == ONBOARDING_CHOICE.ACCEPT_2;
    UrlbarPrefs.set("quicksuggest.dataCollection.enabled", optedIn);

    switch (params.choice) {
      case ONBOARDING_CHOICE.LEARN_MORE_2:
        win.openTrustedLinkIn(UrlbarProviderQuickSuggest.helpUrl, "tab", {
          fromChrome: true,
        });
        break;
      case ONBOARDING_CHOICE.ACCEPT_2:
      case ONBOARDING_CHOICE.REJECT_2:
      case ONBOARDING_CHOICE.NOT_NOW_2:
      case ONBOARDING_CHOICE.CLOSE_1:
        // No other action required.
        break;
      default:
        params.choice = params.visitedMain
          ? ONBOARDING_CHOICE.DISMISS_2
          : ONBOARDING_CHOICE.DISMISS_1;
        break;
    }

    UrlbarPrefs.set("quicksuggest.onboardingDialogChoice", params.choice);

    Services.telemetry.recordEvent(
      "contextservices.quicksuggest",
      "opt_in_dialog",
      params.choice
    );

    return true;
  }

  /**
   * Called when a urlbar pref changes. The onboarding dialog will set the
   * `browser.urlbar.suggest.quicksuggest` prefs if the user has opted in, at
   * which point we can start showing results.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "suggest.quicksuggest.nonsponsored":
      case "suggest.quicksuggest.sponsored":
        this._queueSettingsSetup();
        break;
    }
  }

  // The RemoteSettings client.
  _rs = null;

  // Queue of callback functions for serializing access to remote settings and
  // related data. See _queueSettingsTask().
  _settingsTaskQueue = [];

  // Functions to call when the settings task queue becomes empty.
  _emptySettingsTaskQueueCallbacks = [];

  // Maps from result IDs to the corresponding results.
  _results = new Map();

  // A tree that maps keywords to a result.
  _tree = new KeywordTree();

  /**
   * Queues a task to ensure our remote settings client is initialized or torn
   * down as appropriate.
   */
  _queueSettingsSetup() {
    this._queueSettingsTask(() => {
      let enabled =
        UrlbarPrefs.get(FEATURE_AVAILABLE) &&
        (UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
          UrlbarPrefs.get("suggest.quicksuggest.sponsored"));
      if (enabled && !this._rs) {
        this._onSettingsSync = (...args) => this._queueSettingsSync(...args);
        this._rs = RemoteSettings(RS_COLLECTION);
        this._rs.on("sync", this._onSettingsSync);
        this._queueSettingsSync();
      } else if (!enabled && this._rs) {
        this._rs.off("sync", this._onSettingsSync);
        this._rs = null;
        this._onSettingsSync = null;
      }
    });
  }

  /**
   * Queues a task to (re)create the results map and keyword tree from the
   * remote settings data plus any other work that needs to be done on sync.
   *
   * @param {object} [event]
   *   The event object passed to the "sync" event listener if you're calling
   *   this from the listener.
   */
  _queueSettingsSync(event = null) {
    this._queueSettingsTask(async () => {
      // Remove local files of deleted records
      if (event?.data?.deleted) {
        await Promise.all(
          event.data.deleted
            .filter(d => d.attachment)
            .map(entry => this._rs.attachments.delete(entry))
        );
      }

      let data = await this._rs.get({ filters: { type: "data" } });
      let icons = await this._rs.get({ filters: { type: "icon" } });
      await Promise.all(icons.map(r => this._rs.attachments.download(r)));

      this._results = new Map();
      this._tree = new KeywordTree();

      for (let record of data) {
        let { buffer } = await this._rs.attachments.download(record, {
          useCache: true,
        });
        let results = JSON.parse(new TextDecoder("utf-8").decode(buffer));
        this._addResults(results);
      }
    });
  }

  /**
   * Adds a list of result objects to the results map and keyword tree. This
   * method is also used by tests to set up mock suggestions.
   *
   * @param {array} results
   *   Array of result objects.
   */
  _addResults(results) {
    for (let result of results) {
      this._results.set(result.id, result);
      for (let keyword of result.keywords) {
        this._tree.set(keyword, result.id);
      }
    }
  }

  /**
   * Adds a function to the remote settings task queue. Methods in this class
   * should call this when they need to to modify or access the settings client.
   * It ensures settings accesses are serialized, do not overlap, and happen
   * only one at a time. It also lets clients, especially tests, use this class
   * without having to worry about whether a settings sync or initialization is
   * ongoing; see `readyPromise`.
   *
   * @param {function} callback
   *   The function to queue.
   */
  _queueSettingsTask(callback) {
    this._settingsTaskQueue.push(callback);
    if (this._settingsTaskQueue.length == 1) {
      this._doNextSettingsTask();
    }
  }

  /**
   * Calls the next function in the settings task queue and recurses until the
   * queue is empty. Once empty, all empty-queue callback functions are called.
   */
  async _doNextSettingsTask() {
    if (!this._settingsTaskQueue.length) {
      while (this._emptySettingsTaskQueueCallbacks.length) {
        let callback = this._emptySettingsTaskQueueCallbacks.shift();
        callback();
      }
      return;
    }

    let task = this._settingsTaskQueue[0];
    try {
      await task();
    } catch (error) {
      log.error(error);
    }
    this._settingsTaskQueue.shift();
    this._doNextSettingsTask();
  }

  /**
   * Fetch the icon from RemoteSettings attachments.
   *
   * @param {string} path
   *   The icon's remote settings path.
   */
  async _fetchIcon(path) {
    if (!path || !this._rs) {
      return null;
    }
    let record = (
      await this._rs.get({
        filters: { id: `icon-${path}` },
      })
    ).pop();
    if (!record) {
      return null;
    }
    return this._rs.attachments.download(record);
  }
}

// Token used as a key to store results within the Map, cannot be used
// within a keyword.
const RESULT_KEY = "^";

/**
 * This is an implementation of a Map based Tree. We can store
 * multiple keywords that point to a single term, for example:
 *
 *   tree.add("headphones", "headphones");
 *   tree.add("headph", "headphones");
 *   tree.add("earphones", "headphones");
 *
 *   tree.get("headph") == "headphones"
 *
 * The tree can store multiple prefixes to a term efficiently
 * so ["hea", "head", "headp", "headph", "headpho", ...] wont lead
 * to duplication in memory. The tree will only return a result
 * for keywords that have been explcitly defined and not attempt
 * to guess based on prefix.
 *
 * Once a tree have been build, it can be flattened with `.flatten`
 * the tree can then be serialised and deserialised with `.toJSON`
 * and `.fromJSON`.
 */
class KeywordTree {
  constructor() {
    this.tree = new Map();
  }

  /*
   * Set a keyword for a result.
   */
  set(keyword, id) {
    if (keyword.includes(RESULT_KEY)) {
      throw new Error(`"${RESULT_KEY}" is reserved`);
    }
    let tree = this.tree;
    for (let x = 0, c = ""; (c = keyword.charAt(x)); x++) {
      let child = tree.get(c) || new Map();
      tree.set(c, child);
      tree = child;
    }
    tree.set(RESULT_KEY, id);
  }

  /**
   * Get the result for a given phrase.
   *
   * @param {string} query
   *   The query string.
   * @returns {*}
   *   The matching result in the tree or null if there isn't a match.
   */
  get(query) {
    query = query.trimStart() + RESULT_KEY;
    let node = this.tree;
    let phrase = "";
    while (phrase.length < query.length) {
      // First, assume the tree isn't flattened and try to look up the next char
      // in the query.
      let key = query[phrase.length];
      let child = node.get(key);
      if (!child) {
        // Not found, so fall back to looking through all of the node's keys.
        key = null;
        for (let childKey of node.keys()) {
          let childPhrase = phrase + childKey;
          if (childPhrase == query.substring(0, childPhrase.length)) {
            key = childKey;
            break;
          }
        }
        if (!key) {
          return null;
        }
        child = node.get(key);
      }
      node = child;
      phrase += key;
    }
    if (phrase.length != query.length) {
      return null;
    }
    // At this point, `node` is the found result.
    return node;
  }

  /*
   * We flatten the tree by combining consecutive single branch keywords
   * with the same results into a longer keyword. so ["a", ["b", ["c"]]]
   * becomes ["abc"], we need to be careful that the result matches so
   * if a prefix search for "hello" only starts after 2 characters it will
   * be flattened to ["he", ["llo"]].
   */
  flatten() {
    this._flatten("", this.tree, null);
  }

  /**
   * Recursive flatten() helper.
   *
   * @param {string} key
   *   The key for `node` in `parent`.
   * @param {Map} node
   *   The currently visited node.
   * @param {Map} parent
   *   The parent of `node`, or null if `node` is the root.
   */
  _flatten(key, node, parent) {
    // Flatten the node's children.  We need to store node.entries() in an array
    // rather than iterating over them directly because _flatten() can modify
    // them during iteration.
    for (let [childKey, child] of [...node.entries()]) {
      if (childKey != RESULT_KEY) {
        this._flatten(childKey, child, node);
      }
    }
    // If the node has a single child, then replace the node in `parent` with
    // the child.
    if (node.size == 1 && parent) {
      parent.delete(key);
      let childKey = [...node.keys()][0];
      parent.set(key + childKey, node.get(childKey));
    }
  }

  /*
   * Turn a tree into a serialisable JSON object.
   */
  toJSONObject(map = this.tree) {
    let tmp = {};
    for (let [key, val] of map) {
      if (val instanceof Map) {
        tmp[key] = this.toJSONObject(val);
      } else {
        tmp[key] = val;
      }
    }
    return tmp;
  }

  /*
   * Build a tree from a serialisable JSON object that was built
   * with `toJSON`.
   */
  fromJSON(json) {
    this.tree = this.JSONObjectToMap(json);
  }

  JSONObjectToMap(obj) {
    let map = new Map();
    for (let key of Object.keys(obj)) {
      if (typeof obj[key] == "object") {
        map.set(key, this.JSONObjectToMap(obj[key]));
      } else {
        map.set(key, obj[key]);
      }
    }
    return map;
  }
}

let UrlbarQuickSuggest = new Suggestions();
