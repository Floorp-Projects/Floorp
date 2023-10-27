/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

const RS_COLLECTION = "quicksuggest";

// Entries are added to `SuggestionsMap` map in chunks, and each chunk will add
// at most this many entries.
const SUGGESTIONS_MAP_CHUNK_SIZE = 1000;

const TELEMETRY_LATENCY = "FX_URLBAR_QUICK_SUGGEST_REMOTE_SETTINGS_LATENCY_MS";

// See `SuggestionsMap.MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD`. When a full
// keyword starts with one of the prefixes in this list, the user must type the
// entire prefix to start triggering matches based on that full keyword, instead
// of only the first word.
const KEYWORD_PREFIXES_TO_TREAT_AS_SINGLE_WORDS = ["how to"];

/**
 * The Suggest JS backend. Not used when the Rust backend is enabled.
 */
export class SuggestBackendJs extends BaseFeature {
  constructor(...args) {
    super(...args);
    this.#emitter = new lazy.EventEmitter();
  }

  get shouldEnable() {
    return !lazy.UrlbarPrefs.get("quickSuggestRustEnabled");
  }

  /**
   * @returns {RemoteSettings}
   *   The underlying `RemoteSettings` client object.
   */
  get rs() {
    return this.#rs;
  }

  /**
   * @returns {EventEmitter}
   *   The client will emit events on this object.
   */
  get emitter() {
    return this.#emitter;
  }

  /**
   * @returns {object}
   *   Global quick suggest configuration stored in remote settings. When the
   *   config changes the `emitter` property will emit a "config-set" event. The
   *   config is an object that looks like this:
   *
   *   {
   *     impression_caps: {
   *       nonsponsored: {
   *         lifetime,
   *         custom: [
   *           { interval_s, max_count },
   *         ],
   *       },
   *       sponsored: {
   *         lifetime,
   *         custom: [
   *           { interval_s, max_count },
   *         ],
   *       },
   *     },
   *     show_less_frequently_cap,
   *   }
   */
  get config() {
    return this.#config;
  }

  /**
   * @returns {Array}
   *   Array of `BasicFeature` instances.
   */
  get features() {
    return [...this.#features];
  }

  enable(enabled) {
    if (!enabled) {
      this.#enableSettings(false);
    } else if (this.#features.size) {
      this.#enableSettings(true);
      this.#syncAll();
    }
  }

  /**
   * Registers a quick suggest feature that uses remote settings.
   *
   * @param {BaseFeature} feature
   *   An instance of a `BaseFeature` subclass. See `BaseFeature` for methods
   *   that the subclass must implement.
   */
  register(feature) {
    this.logger.debug("Registering feature: " + feature.name);
    this.#features.add(feature);
    if (this.isEnabled) {
      if (this.#features.size == 1) {
        this.#enableSettings(true);
      }
      this.#syncFeature(feature);
    }
  }

  /**
   * Unregisters a quick suggest feature that uses remote settings.
   *
   * @param {BaseFeature} feature
   *   An instance of a `BaseFeature` subclass.
   */
  unregister(feature) {
    this.logger.debug("Unregistering feature: " + feature.name);
    this.#features.delete(feature);
    if (!this.#features.size) {
      this.#enableSettings(false);
    }
  }

  /**
   * Queries remote settings suggestions from all registered features.
   *
   * @param {string} searchString
   *   The search string.
   * @returns {Array}
   *   The remote settings suggestions. If there are no matches, an empty array
   *   is returned.
   */
  async query(searchString) {
    let suggestions;
    let stopwatchInstance = {};
    TelemetryStopwatch.start(TELEMETRY_LATENCY, stopwatchInstance);
    try {
      suggestions = await this.#queryHelper(searchString);
      TelemetryStopwatch.finish(TELEMETRY_LATENCY, stopwatchInstance);
    } catch (error) {
      TelemetryStopwatch.cancel(TELEMETRY_LATENCY, stopwatchInstance);
      this.logger.error("Query error: " + error);
    }

    return suggestions || [];
  }

  async #queryHelper(searchString) {
    this.logger.info("Handling query: " + JSON.stringify(searchString));

    let results = await Promise.all(
      [...this.#features].map(async feature => {
        let suggestions = await feature.queryRemoteSettings(searchString);
        return [feature, suggestions ?? []];
      })
    );

    let allSuggestions = [];
    for (let [feature, suggestions] of results) {
      for (let suggestion of suggestions) {
        // Features typically return suggestion objects straight from their
        // suggestion maps. We don't want consumers to modify those objects
        // since they are the source of truth (tests especially tend to do
        // this), so return copies to consumers.
        allSuggestions.push({
          ...suggestion,
          source: "remote-settings",
          provider: feature.name,
        });
      }
    }

    return allSuggestions;
  }

  async #enableSettings(enabled) {
    if (enabled && !this.#rs) {
      this.logger.debug("Creating RemoteSettings client");
      this.#onSettingsSync = event => this.#syncAll({ event });
      this.#rs = lazy.RemoteSettings(RS_COLLECTION);
      this.#rs.on("sync", this.#onSettingsSync);
      await this.#syncConfig();
    } else if (!enabled && this.#rs) {
      this.logger.debug("Destroying RemoteSettings client");
      this.#rs.off("sync", this.#onSettingsSync);
      this.#rs = null;
      this.#onSettingsSync = null;
    }
  }

  async #syncConfig() {
    this.logger.debug("Syncing config");
    let rs = this.#rs;

    let configArray = await rs.get({ filters: { type: "configuration" } });
    if (rs != this.#rs) {
      return;
    }

    this.logger.debug("Got config array: " + JSON.stringify(configArray));
    this.#setConfig(configArray?.[0]?.configuration || {});
  }

  async #syncFeature(feature) {
    this.logger.debug("Syncing feature: " + feature.name);
    await feature.onRemoteSettingsSync(this.#rs);
  }

  async #syncAll({ event = null } = {}) {
    this.logger.debug("Syncing all");
    let rs = this.#rs;

    // Remove local files of deleted records
    if (event?.data?.deleted) {
      await Promise.all(
        event.data.deleted
          .filter(d => d.attachment)
          .map(entry =>
            Promise.all([
              this.#rs.attachments.deleteDownloaded(entry), // type: data
              this.#rs.attachments.deleteFromDisk(entry), // type: icon
            ])
          )
      );
      if (rs != this.#rs) {
        return;
      }
    }

    let promises = [this.#syncConfig()];
    for (let feature of this.#features) {
      promises.push(this.#syncFeature(feature));
    }
    await Promise.all(promises);
  }

  /**
   * Sets the quick suggest config and emits a "config-set" event.
   *
   * @param {object} config
   *   The config object.
   */
  #setConfig(config) {
    config ??= {};
    this.logger.debug("Setting config: " + JSON.stringify(config));
    this.#config = config;
    this.#emitter.emit("config-set");
  }

  async _test_syncAll() {
    if (this.#rs) {
      // `RemoteSettingsClient` won't start another import if it's already
      // importing. Wait for it to finish before starting the new one.
      await this.#rs._importingPromise;
      await this.#syncAll();
    }
  }

  // The `RemoteSettings` client.
  #rs = null;

  // Registered `BaseFeature` instances.
  #features = new Set();

  // Configuration data synced from remote settings. See the `config` getter.
  #config = {};

  #emitter = null;
  #logger = null;
  #onSettingsSync = null;
}

/**
 * A wrapper around `Map` that handles quick suggest suggestions from remote
 * settings. It maps keywords to suggestions. It has two benefits over `Map`:
 *
 * - The main benefit is that map entries are added in batches on idle to avoid
 *   blocking the main thread for too long, since there can be many suggestions
 *   and keywords.
 * - A secondary benefit is that the interface is tailored to quick suggest
 *   suggestions, which have a `keywords` property.
 */
export class SuggestionsMap {
  /**
   * Returns the list of suggestions for a keyword.
   *
   * @param {string} keyword
   *   The keyword.
   * @returns {Array}
   *   The array of suggestions for the keyword. If the keyword isn't in the
   *   map, the array will be empty.
   */
  get(keyword) {
    let object = this.#suggestionsByKeyword.get(keyword.toLocaleLowerCase());
    if (!object) {
      return [];
    }
    return Array.isArray(object) ? object : [object];
  }

  /**
   * Adds a list of suggestion objects to the results map. Each suggestion must
   * have a property whose value is an array of keyword strings. The
   * suggestion's keywords will be taken from this array either exactly as they
   * are specified or by generating new keywords from them; see `mapKeyword`.
   *
   * @param {Array} suggestions
   *   Array of suggestion objects.
   * @param {object} options
   *   Options object.
   * @param {string} options.keywordsProperty
   *   The name of the keywords property in each suggestion.
   * @param {Function} options.mapKeyword
   *   If null, the keywords for each suggestion will be taken from the keywords
   *   array exactly as they are specified. Otherwise, this function will be
   *   called for each string in the array, and it should return an array of
   *   strings. The suggestion's final list of keywords will be the union of all
   *   strings returned by this function. See also the `MAP_KEYWORD_*` consts.
   */
  async add(
    suggestions,
    { keywordsProperty = "keywords", mapKeyword = null } = {}
  ) {
    // There can be many suggestions, and each suggestion can have many
    // keywords. To avoid blocking the main thread for too long, update the map
    // in chunks, and to avoid blocking the UI and other higher priority work,
    // do each chunk only when the main thread is idle. During each chunk, we'll
    // add at most `chunkSize` entries to the map.
    let suggestionIndex = 0;
    let keywordIndex = 0;

    // Keep adding chunks until all suggestions have been fully added.
    while (suggestionIndex < suggestions.length) {
      await new Promise(resolve => {
        Services.tm.idleDispatchToMainThread(() => {
          // Keep updating the map until the current chunk is done.
          let indexInChunk = 0;
          while (
            indexInChunk < SuggestionsMap.chunkSize &&
            suggestionIndex < suggestions.length
          ) {
            let suggestion = suggestions[suggestionIndex];
            let keywords = suggestion[keywordsProperty];
            if (keywordIndex == keywords.length) {
              // We've added entries for all keywords of the current suggestion.
              // Move on to the next suggestion.
              suggestionIndex++;
              keywordIndex = 0;
              continue;
            }

            // As a convenience, allow `mapKeyword` to return a string even
            // though the JSDoc says an array must be returned.
            let originalKeyword = keywords[keywordIndex];
            let mappedKeywords =
              mapKeyword?.(originalKeyword) ?? originalKeyword;
            if (typeof mappedKeywords == "string") {
              mappedKeywords = [mappedKeywords];
            }

            for (let keyword of mappedKeywords) {
              // If the keyword's only suggestion is `suggestion`, store it
              // directly as the value. Otherwise store an array of unique
              // suggestions. See the `#suggestionsByKeyword` comment.
              let object = this.#suggestionsByKeyword.get(keyword);
              if (!object) {
                this.#suggestionsByKeyword.set(keyword, suggestion);
              } else {
                let isArray = Array.isArray(object);
                if (!isArray && object != suggestion) {
                  this.#suggestionsByKeyword.set(keyword, [object, suggestion]);
                } else if (isArray && !object.includes(suggestion)) {
                  object.push(suggestion);
                }
              }
            }

            keywordIndex++;
            indexInChunk++;
          }

          // The current chunk is done.
          resolve();
        });
      });
    }
  }

  clear() {
    this.#suggestionsByKeyword.clear();
  }

  /**
   * @returns {Function}
   *   A `mapKeyword` function that maps a keyword to an array containing the
   *   keyword's first word plus every subsequent prefix of the keyword. The
   *   strings in `KEYWORD_PREFIXES_TO_TREAT_AS_SINGLE_WORDS` will modify this
   *   behavior: When a full keyword starts with one of the prefixes in that
   *   list, the generated prefixes will start at that prefix instead of the
   *   first word.
   */
  static get MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD() {
    return fullKeyword => {
      let prefix = KEYWORD_PREFIXES_TO_TREAT_AS_SINGLE_WORDS.find(p =>
        fullKeyword.startsWith(p + " ")
      );
      let spaceIndex = prefix ? prefix.length : fullKeyword.indexOf(" ");

      let keywords = [fullKeyword];
      if (spaceIndex >= 0) {
        for (let i = spaceIndex; i < fullKeyword.length; i++) {
          keywords.push(fullKeyword.substring(0, i));
        }
      }
      return keywords;
    };
  }

  // Maps each keyword in the dataset to one or more suggestions for the
  // keyword. If only one suggestion uses a keyword, the keyword's value in the
  // map will be the suggestion object. If more than one suggestion uses the
  // keyword, the value will be an array of the suggestions. The reason for not
  // always using an array is that we expect the vast majority of keywords to be
  // used by only one suggestion, and since there are potentially very many
  // keywords and suggestions and we keep them in memory all the time, we want
  // to save as much memory as possible.
  #suggestionsByKeyword = new Map();

  // This is only defined as a property so that tests can override it.
  static chunkSize = SUGGESTIONS_MAP_CHUNK_SIZE;
}
