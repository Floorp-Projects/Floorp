/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarQuickSuggest", "KeywordTree"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["TextDecoder"]);

const log = console.createInstance({
  prefix: "QuickSuggest",
  maxLogLevel: UrlbarPrefs.get("quicksuggest.log") ? "All" : "Warn",
});

const RS_COLLECTION = "quicksuggest";
const RS_PREF = "quicksuggest.enabled";

// Categories that should show "Firefox Suggest" instead of "Sponsored"
const NONSPONSORED_IAB_CATEGORIES = new Set(["5 - Education"]);

/**
 * Fetches the suggestions data from RemoteSettings and builds the tree
 * to provide suggestions for UrlbarProviderQuickSuggest.
 */
class Suggestions {
  // The RemoteSettings client.
  _rs = null;
  // Let tests wait for init to complete.
  _initPromise = null;
  // Resolver function stored to call when init is complete.
  _initResolve = null;
  // A tree that maps keywords to a result.
  _tree = new KeywordTree();
  // A map of the result data.
  _results = new Map();

  async init() {
    if (this._initPromise) {
      return this._initPromise;
    }
    this._initPromise = Promise.resolve();
    this._rs = RemoteSettings(RS_COLLECTION);
    if (UrlbarPrefs.get(RS_PREF)) {
      this._initPromise = new Promise(resolve => (this._initResolve = resolve));
      Services.tm.idleDispatchToMainThread(
        this._setupRemoteSettings.bind(this)
      );
    } else {
      UrlbarPrefs.addObserver(this);
    }
    return this._initPromise;
  }

  /*
   * Handle queries from the Urlbar.
   */
  async query(phrase) {
    log.info("Handling query for", phrase);
    phrase = phrase.toLowerCase();
    let match = this._tree.get(phrase);
    if (!match.result || !this._results.has(match.result)) {
      return null;
    }
    let result = this._results.get(match.result);
    let d = new Date();
    let pad = number => number.toString().padStart(2, "0");
    let date =
      `${d.getFullYear()}${pad(d.getMonth() + 1)}` +
      `${pad(d.getDate())}${pad(d.getHours())}`;
    let icon = await this.fetchIcon(result.icon);
    return {
      fullKeyword: match.fullKeyword,
      title: result.title,
      url: result.url.replace("%YYYYMMDDHH%", date),
      click_url: result.click_url.replace("%YYYYMMDDHH%", date),
      // impression_url doesn't have any parameters
      impression_url: result.impression_url,
      block_id: result.id,
      advertiser: result.advertiser.toLocaleLowerCase(),
      isSponsored: !NONSPONSORED_IAB_CATEGORIES.has(result.iab_category),
      icon,
    };
  }

  /*
   * Called if a Urlbar preference is changed.
   */
  onPrefChanged(changedPref) {
    if (changedPref == RS_PREF && UrlbarPrefs.get(RS_PREF)) {
      this._setupRemoteSettings();
    }
  }

  /*
   * Set up RemoteSettings listeners.
   */
  async _setupRemoteSettings() {
    this._rs.on("sync", this._onSettingsSync.bind(this));
    await this._ensureAttachmentsDownloaded();
    if (this._initResolve) {
      this._initResolve();
      this._initResolve = null;
    }
  }

  /*
   * Called when RemoteSettings updates are received.
   */
  async _onSettingsSync({ data: { deleted } }) {
    const toDelete = deleted?.filter(d => d.attachment);
    // Remove local files of deleted records
    if (toDelete) {
      await Promise.all(
        toDelete.map(entry => this._rs.attachments.delete(entry))
      );
    }
    await this._ensureAttachmentsDownloaded();
  }

  /*
   * We store our RemoteSettings data in attachments, ensure the attachments
   * are saved locally.
   */
  async _ensureAttachmentsDownloaded() {
    log.info("_ensureAttachmentsDownloaded started");
    let dataOpts = { useCache: true };
    let data = await this._rs.get({ filters: { type: "data" } });
    await Promise.all(
      data.map(r => this._rs.attachments.download(r, dataOpts))
    );

    let icons = await this._rs.get({ filters: { type: "icon" } });
    await Promise.all(icons.map(r => this._rs.attachments.download(r)));

    await this._createTree();
    log.info("_ensureAttachmentsDownloaded complete");
  }

  /*
   * Recreate the KeywordTree on startup or with RemoteSettings updates.
   */
  async _createTree() {
    log.info("Building new KeywordTree");
    this._results = new Map();
    this._tree = new KeywordTree();
    let data = await this._rs.get({ filters: { type: "data" } });

    for (let record of data) {
      let { buffer } = await this._rs.attachments.download(record, {
        useCache: true,
      });
      let json = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      this._processSuggestionsJSON(json);
    }
  }

  /*
   * Handle incoming suggestions data and add to local data.
   */
  async _processSuggestionsJSON(json) {
    for (let result of json) {
      this._results.set(result.id, result);
      for (let keyword of result.keywords) {
        this._tree.set(keyword, result.id);
      }
    }
  }

  /*
   * Fetch the icon from RemoteSettings attachments.
   */
  async fetchIcon(path) {
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
   * @returns {object}
   *   The match object: { result, fullKeyword }, where `result` is the matching
   *   result ID and `fullKeyword` is the suggestion.  If there is no match,
   *   then `result` will be null and `fullKeyword` will not be defined.
   */
  get(query) {
    query = query.trimStart();
    let match = this._getMatch(query, this.tree, "");
    if (!match) {
      return { result: null };
    }

    let result = UrlbarQuickSuggest._results.get(match.resultID);
    if (!result) {
      return { result: null };
    }

    let longerPhrase;
    let trimmedQuery = query.trim();
    let queryWords = trimmedQuery.split(" ");

    // We need to determine a suggestion for the result (called `fullKeyword` in
    // the returned object).  The data doesn't include suggestions, so we'll
    // need to make our own based on the keyword phrases in the result.  First,
    // find a decent matching phrase.  We'll use two heuristics:
    //
    // (1) Find the first phrase that has more words than the query.  Use its
    //     first `queryWords.length` words as the suggestion.  e.g., if the
    //     query is "moz" and `result.keywords` is ["moz", "mozi", "mozil",
    //     "mozill", "mozilla", "mozilla firefox"], pick "mozilla firefox", pop
    //     off the "firefox" and use "mozilla" as the suggestion.
    // (2) If there isn't any phrase with more words, then pick the longest
    //     phrase.  e.g., pick "mozilla" in the previous example (assuming the
    //     "mozilla firefox" phrase isn't there).
    for (let phrase of result.keywords) {
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
          return {
            result: match.resultID,
            fullKeyword: phraseWords.slice(0, len).join(" "),
          };
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
    return {
      result: match.resultID,
      fullKeyword: longerPhrase || trimmedQuery,
    };
  }

  /**
   * Recursively looks up a match in the tree.
   *
   * @param {string} query
   *   The query string.
   * @param {Map} node
   *   The node to start the search at.
   * @param {string} phrase
   *   The current phrase key path through the tree.
   * @returns {object}
   *   The match object: { resultID }, or null if no match was found.
   */
  _getMatch(query, node, phrase) {
    for (const [key, child] of node.entries()) {
      if (key == RESULT_KEY) {
        continue;
      }
      let newPhrase = phrase + key;
      let len = Math.min(newPhrase.length, query.length);
      if (newPhrase.substring(0, len) == query.substring(0, len)) {
        // The new phrase is a prefix of the query or vice versa.
        let resultID = child.get(RESULT_KEY);
        if (resultID !== undefined) {
          // This child has a result.  Look it up to see if its keyword phrases
          // match the query.  If it has a phrase that starts with the query,
          // then it potentially matches, but we don't want to match when the
          // query is shorter than every phrase, so also check that the query
          // starts with some phrase.  For example, if the query is "m" and the
          // phrases are ["moz", "mozilla"], we don't want to match until the
          // query is at least "moz".
          let result = UrlbarQuickSuggest._results.get(resultID);
          if (
            result &&
            result.keywords.some(p => p.startsWith(query)) &&
            result.keywords.some(p => query.startsWith(p))
          ) {
            return { resultID };
          }
        }
        // Recurse into descendants and return any match.
        let match = this._getMatch(query, child, newPhrase);
        if (match) {
          return match;
        }
      }
    }
    return null;
  }

  /*
   * We flatten the tree by combining consecutive single branch keywords
   * with the same results into a longer keyword. so ["a", ["b", ["c"]]]
   * becomes ["abc"], we need to be careful that the result matches so
   * if a prefix search for "hello" only starts after 2 characters it will
   * be flattened to ["he", ["llo"]].
   */
  flatten() {
    for (let key of Array.from(this.tree.keys())) {
      this._flatten(this.tree, key);
    }
  }

  _flatten(parent, key) {
    let tree = parent.get(key);
    let keys = Array.from(tree.keys()).filter(k => k != RESULT_KEY);
    let result = tree.get(RESULT_KEY);

    if (keys.length == 1) {
      let childKey = keys[0];
      let child = tree.get(childKey);
      let childResult = child.get(RESULT_KEY);

      if (result == childResult) {
        let newKey = key + childKey;
        parent.set(newKey, child);
        parent.delete(key);
        this._flatten(parent, newKey);
      } else {
        this._flatten(tree, childKey);
      }
    } else {
      for (let k of keys) {
        this._flatten(tree, k);
      }
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
UrlbarQuickSuggest.init();
