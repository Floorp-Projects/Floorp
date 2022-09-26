/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QUICK_SUGGEST_SOURCE:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  TaskQueue: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const log = console.createInstance({
  prefix: "QuickSuggest",
  maxLogLevel: lazy.UrlbarPrefs.get("quicksuggest.log") ? "All" : "Warn",
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
export const ONBOARDING_CHOICE = {
  ACCEPT_2: "accept_2",
  CLOSE_1: "close_1",
  DISMISS_1: "dismiss_1",
  DISMISS_2: "dismiss_2",
  LEARN_MORE_1: "learn_more_1",
  LEARN_MORE_2: "learn_more_2",
  NOT_NOW_2: "not_now_2",
  REJECT_2: "reject_2",
};

const ONBOARDING_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.html";

// This is a score in the range [0, 1] used by the provider to compare
// suggestions. All suggestions require a score, so if a remote settings
// suggestion does not have one, it's assigned this value. We choose a low value
// to allow Merino to experiment with a broad range of scores server side.
const DEFAULT_SUGGESTION_SCORE = 0.2;

// Entries are added to the `_resultsByKeyword` map in chunks, and each chunk
// will add at most this many entries.
const ADD_RESULTS_CHUNK_SIZE = 1000;

/**
 * Fetches the suggestions data from RemoteSettings and builds the structures
 * to provide suggestions for UrlbarProviderQuickSuggest.
 */
class QuickSuggest extends EventEmitter {
  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    lazy.UrlbarPrefs.addObserver(this);
    lazy.NimbusFeatures.urlbar.onUpdate(() => this._queueSettingsSetup());
    this._queueSettingsSetup();
  }

  /**
   * @returns {number}
   *   A score in the range [0, 1] that can be used to compare suggestions. All
   *   suggestions require a score, so if a remote settings suggestion does not
   *   have one, it's assigned this value.
   */
  get DEFAULT_SUGGESTION_SCORE() {
    return DEFAULT_SUGGESTION_SCORE;
  }

  /**
   * @returns {Promise}
   *   Resolves when any ongoing updates to the suggestions data are done.
   */
  get readyPromise() {
    return this._settingsTaskQueue.emptyPromise;
  }

  /**
   * @returns {object}
   *   Global quick suggest configuration from remote settings:
   *
   *   {
   *     best_match: {
   *       min_search_string_length,
   *       blocked_suggestion_ids,
   *     },
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
   *   }
   */
  get config() {
    return this._config;
  }

  /**
   * Handle queries from the Urlbar.
   *
   * @param {string} phrase
   *   The search string.
   * @returns {array}
   *   The matched suggestion objects. If there are no matches, an empty array
   *   is returned.
   */
  async query(phrase) {
    log.info("Handling query for", phrase);
    phrase = phrase.toLowerCase();
    let object = this._resultsByKeyword.get(phrase);
    if (!object) {
      return [];
    }

    // `object` will be a single result object if there's only one match or an
    // array of result objects if there's more than one match.
    let results = [object].flat();

    // Start each icon fetch at the same time and wait for them all to finish.
    let icons = await Promise.all(
      results.map(({ icon }) => this._fetchIcon(icon))
    );

    return results.map(result => ({
      full_keyword: this.getFullKeyword(phrase, result.keywords),
      title: result.title,
      url: result.url,
      click_url: result.click_url,
      impression_url: result.impression_url,
      block_id: result.id,
      advertiser: result.advertiser,
      iab_category: result.iab_category,
      is_sponsored: !NONSPONSORED_IAB_CATEGORIES.has(result.iab_category),
      score:
        typeof result.score == "number"
          ? result.score
          : DEFAULT_SUGGESTION_SCORE,
      source: lazy.QUICK_SUGGEST_SOURCE.REMOTE_SETTINGS,
      icon: icons.shift(),
      position: result.position,
      _test_is_best_match: result._test_is_best_match,
    }));
  }

  /**
   * Records the Nimbus exposure event if it hasn't already been recorded during
   * the app session. This method actually queues the recording on idle because
   * it's potentially an expensive operation.
   */
  ensureExposureEventRecorded() {
    // `recordExposureEvent()` makes sure only one event is recorded per app
    // session even if it's called many times, but since it may be expensive, we
    // also keep `_recordedExposureEvent`.
    if (!this._recordedExposureEvent) {
      this._recordedExposureEvent = true;
      Services.tm.idleDispatchToMainThread(() =>
        lazy.NimbusFeatures.urlbar.recordExposureEvent({ once: true })
      );
    }
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
    await lazy.UrlbarPrefs.firefoxSuggestScenarioStartupPromise;

    // If the feature is disabled, the user has already seen the dialog, or the
    // user has already opted in, don't show the onboarding.
    if (
      !lazy.UrlbarPrefs.get(FEATURE_AVAILABLE) ||
      lazy.UrlbarPrefs.get(SEEN_DIALOG_PREF) ||
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled")
    ) {
      return false;
    }

    // Wait a number of restarts before showing the dialog.
    let restartsSeen = lazy.UrlbarPrefs.get(RESTARTS_PREF);
    if (
      restartsSeen <
      lazy.UrlbarPrefs.get("quickSuggestShowOnboardingDialogAfterNRestarts")
    ) {
      lazy.UrlbarPrefs.set(RESTARTS_PREF, restartsSeen + 1);
      return false;
    }

    let win = lazy.BrowserWindowTracker.getTopWindow();

    // Don't show the dialog on top of about:welcome for new users.
    if (win.gBrowser?.currentURI?.spec == "about:welcome") {
      return false;
    }

    if (lazy.UrlbarPrefs.get("experimentType") === "modal") {
      this.ensureExposureEventRecorded();
    }

    if (!lazy.UrlbarPrefs.get("quickSuggestShouldShowOnboardingDialog")) {
      return false;
    }

    let variationType;
    try {
      // An error happens if the pref is not in user prefs.
      variationType = lazy.UrlbarPrefs.get(DIALOG_VARIATION_PREF).toLowerCase();
    } catch (e) {}

    let params = { choice: undefined, variationType, visitedMain: false };
    await win.gDialogBox.open(ONBOARDING_URI, params);

    lazy.UrlbarPrefs.set(SEEN_DIALOG_PREF, true);
    lazy.UrlbarPrefs.set(
      DIALOG_VERSION_PREF,
      JSON.stringify({ version: 1, variation: variationType })
    );

    // Record the user's opt-in choice on the user branch. This pref is sticky,
    // so it will retain its user-branch value regardless of what the particular
    // default was at the time.
    let optedIn = params.choice == ONBOARDING_CHOICE.ACCEPT_2;
    lazy.UrlbarPrefs.set("quicksuggest.dataCollection.enabled", optedIn);

    switch (params.choice) {
      case ONBOARDING_CHOICE.LEARN_MORE_1:
      case ONBOARDING_CHOICE.LEARN_MORE_2:
        win.openTrustedLinkIn(lazy.UrlbarProviderQuickSuggest.helpUrl, "tab", {
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

    lazy.UrlbarPrefs.set("quicksuggest.onboardingDialogChoice", params.choice);

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

  _initialized = false;

  // The RemoteSettings client.
  _rs = null;

  // Task queue for serializing access to remote settings and related data.
  // Methods in this class should use this when they need to to modify or access
  // the settings client. It ensures settings accesses are serialized, do not
  // overlap, and happen only one at a time. It also lets clients, especially
  // tests, use this class without having to worry about whether a settings sync
  // or initialization is ongoing; see `readyPromise`.
  _settingsTaskQueue = new lazy.TaskQueue();

  // Configuration data synced from remote settings. See the `config` getter.
  _config = {};

  // Maps each keyword in the dataset to one or more results for the keyword. If
  // only one result uses a keyword, the keyword's value in the map will be the
  // result object. If more than one result uses the keyword, the value will be
  // an array of the results. The reason for not always using an array is that
  // we expect the vast majority of keywords to be used by only one result, and
  // since there are potentially very many keywords and results and we keep them
  // in memory all the time, we want to save as much memory as possible.
  _resultsByKeyword = new Map();

  // This is only defined as a property so that tests can override it.
  _addResultsChunkSize = ADD_RESULTS_CHUNK_SIZE;

  /**
   * Queues a task to ensure our remote settings client is initialized or torn
   * down as appropriate.
   */
  _queueSettingsSetup() {
    this._settingsTaskQueue.queue(() => {
      let enabled =
        lazy.UrlbarPrefs.get(FEATURE_AVAILABLE) &&
        (lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
          lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored"));
      if (enabled && !this._rs) {
        this._onSettingsSync = (...args) => this._queueSettingsSync(...args);
        this._rs = lazy.RemoteSettings(RS_COLLECTION);
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
   * Queues a task to populate the results map from the remote settings data
   * plus any other work that needs to be done on sync.
   *
   * @param {object} [event]
   *   The event object passed to the "sync" event listener if you're calling
   *   this from the listener.
   */
  async _queueSettingsSync(event = null) {
    await this._settingsTaskQueue.queue(async () => {
      // Remove local files of deleted records
      if (event?.data?.deleted) {
        await Promise.all(
          event.data.deleted
            .filter(d => d.attachment)
            .map(entry =>
              Promise.all([
                this._rs.attachments.deleteDownloaded(entry), // type: data
                this._rs.attachments.deleteFromDisk(entry), // type: icon
              ])
            )
        );
      }

      let dataType = lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsDataType");
      log.debug("Loading data with type:", dataType);

      let [configArray, data] = await Promise.all([
        this._rs.get({ filters: { type: "configuration" } }),
        this._rs.get({ filters: { type: dataType } }),
        this._rs
          .get({ filters: { type: "icon" } })
          .then(icons =>
            Promise.all(icons.map(i => this._rs.attachments.downloadToDisk(i)))
          ),
      ]);

      log.debug("Got configuration:", configArray);
      this._setConfig(configArray?.[0]?.configuration || {});

      this._resultsByKeyword.clear();

      log.debug(`Got data with ${data.length} records`);
      for (let record of data) {
        let { buffer } = await this._rs.attachments.download(record);
        let results = JSON.parse(new TextDecoder("utf-8").decode(buffer));
        log.debug(`Adding ${results.length} results`);
        await this._addResults(results);
      }
    });
  }

  /**
   * Sets the quick suggest config and emits a "config-set" event.
   *
   * @param {object} config
   */
  _setConfig(config) {
    this._config = config || {};
    this.emit("config-set");
  }

  /**
   * Adds a list of result objects to the results map. This method is also used
   * by tests to set up mock suggestions.
   *
   * @param {array} results
   *   Array of result objects.
   */
  async _addResults(results) {
    // There can be many results, and each result can have many keywords. To
    // avoid blocking the main thread for too long, update the map in chunks,
    // and to avoid blocking the UI and other higher priority work, do each
    // chunk only when the main thread is idle. During each chunk, we'll add at
    // most `_addResultsChunkSize` entries to the map.
    let resultIndex = 0;
    let keywordIndex = 0;

    // Keep adding chunks until all results have been fully added.
    while (resultIndex < results.length) {
      await new Promise(resolve => {
        Services.tm.idleDispatchToMainThread(() => {
          // Keep updating the map until the current chunk is done.
          let indexInChunk = 0;
          while (
            indexInChunk < this._addResultsChunkSize &&
            resultIndex < results.length
          ) {
            let result = results[resultIndex];
            if (keywordIndex == result.keywords.length) {
              resultIndex++;
              keywordIndex = 0;
              continue;
            }
            // If the keyword's only result is `result`, store it directly as
            // the value. Otherwise store an array of results. For details, see
            // the `_resultsByKeyword` comment.
            let keyword = result.keywords[keywordIndex];
            let object = this._resultsByKeyword.get(keyword);
            if (!object) {
              this._resultsByKeyword.set(keyword, result);
            } else if (!Array.isArray(object)) {
              this._resultsByKeyword.set(keyword, [object, result]);
            } else {
              object.push(result);
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
    return this._rs.attachments.downloadToDisk(record);
  }
}

export let UrlbarQuickSuggest = new QuickSuggest();
