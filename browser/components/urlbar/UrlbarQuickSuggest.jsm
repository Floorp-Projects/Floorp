/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ONBOARDING_CHOICE", "UrlbarQuickSuggest"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  QUICK_SUGGEST_SOURCE: "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  Services: "resource://gre/modules/Services.jsm",
  TaskQueue: "resource:///modules/UrlbarUtils.jsm",
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
 * Fetches the suggestions data from RemoteSettings and builds the structures
 * to provide suggestions for UrlbarProviderQuickSuggest.
 */
class Suggestions {
  constructor() {
    UrlbarPrefs.addObserver(this);
    NimbusFeatures.urlbar.onUpdate(() => this._queueSettingsSetup());

    this._settingsTaskQueue.queue(() => {
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
    return this._settingsTaskQueue.emptyPromise;
  }

  /**
   * @returns {object}
   *   Global quick suggest configuration from remote settings:
   *   {
   *     best_match: {
   *       min_search_string_length,
   *       blocked_suggestion_ids,
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
   * @returns {object}
   *   The matched suggestion object or null if none.
   */
  async query(phrase) {
    log.info("Handling query for", phrase);
    phrase = phrase.toLowerCase();
    let result = this._resultsByKeyword.get(phrase);
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
      _test_is_best_match: result._test_is_best_match,
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

    // Don't show the dialog on top of about:welcome for new users.
    if (win.gBrowser?.currentURI?.spec == "about:welcome") {
      return false;
    }

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

  // Task queue for serializing access to remote settings and related data.
  // Methods in this class should use this when they need to to modify or access
  // the settings client. It ensures settings accesses are serialized, do not
  // overlap, and happen only one at a time. It also lets clients, especially
  // tests, use this class without having to worry about whether a settings sync
  // or initialization is ongoing; see `readyPromise`.
  _settingsTaskQueue = new TaskQueue();

  // Configuration data synced from remote settings.
  _config = {};

  // Maps from keywords to their corresponding results. Keywords are unique in
  // the underlying data, so a keyword will only ever map to one result.
  _resultsByKeyword = new Map();

  /**
   * Queues a task to ensure our remote settings client is initialized or torn
   * down as appropriate.
   */
  _queueSettingsSetup() {
    this._settingsTaskQueue.queue(() => {
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
   * Queues a task to populate the results map from the remote settings data
   * plus any other work that needs to be done on sync.
   *
   * @param {object} [event]
   *   The event object passed to the "sync" event listener if you're calling
   *   this from the listener.
   */
  _queueSettingsSync(event = null) {
    this._settingsTaskQueue.queue(async () => {
      // Remove local files of deleted records
      if (event?.data?.deleted) {
        await Promise.all(
          event.data.deleted
            .filter(d => d.attachment)
            .map(entry => this._rs.attachments.delete(entry))
        );
      }

      let [configArray, data] = await Promise.all([
        this._rs.get({ filters: { type: "configuration" } }),
        this._rs.get({ filters: { type: "data" } }),
        this._rs
          .get({ filters: { type: "icon" } })
          .then(icons =>
            Promise.all(icons.map(i => this._rs.attachments.download(i)))
          ),
      ]);

      log.debug("Got configuration:", configArray);
      this._config = configArray?.[0]?.configuration || {};

      this._resultsByKeyword.clear();

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
   * Adds a list of result objects to the results map. This method is also used
   * by tests to set up mock suggestions.
   *
   * @param {array} results
   *   Array of result objects.
   */
  _addResults(results) {
    for (let result of results) {
      for (let keyword of result.keywords) {
        this._resultsByKeyword.set(keyword, result);
      }
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
    return this._rs.attachments.download(record);
  }
}

let UrlbarQuickSuggest = new Suggestions();
