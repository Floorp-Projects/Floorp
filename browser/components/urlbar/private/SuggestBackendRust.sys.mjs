/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  RemoteSettingsConfig: "resource://gre/modules/RustRemoteSettings.sys.mjs",
  SuggestIngestionConstraints: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestStore: "resource://gre/modules/RustSuggest.sys.mjs",
  Suggestion: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionProvider: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionQuery: "resource://gre/modules/RustSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  Utils: "resource://services-settings/Utils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const SUGGEST_STORE_BASENAME = "suggest.sqlite";

// This ID is used to register our ingest timer with nsIUpdateTimerManager.
const INGEST_TIMER_ID = "suggest-ingest";
const INGEST_TIMER_LAST_UPDATE_PREF = `app.update.lastUpdateTime.${INGEST_TIMER_ID}`;

// Maps from `suggestion.constructor` to the corresponding name of the
// suggestion type. See `getSuggestionType()` for details.
const gSuggestionTypesByCtor = new WeakMap();

/**
 * The Suggest Rust backend. Not used when the remote settings JS backend is
 * enabled.
 *
 * This class returns suggestions served by the Rust component. These are the
 * primary related architectural pieces (see bug 1851256 for details):
 *
 * (1) The `suggest` Rust component, which lives in the application-services
 *     repo [1] and is periodically vendored into mozilla-central [2] and then
 *     built into the Firefox binary.
 * (2) `suggest.udl`, which is part of the Rust component's source files and
 *     defines the interface exposed to foreign-function callers like JS [3, 4].
 * (3) `RustSuggest.sys.mjs` [5], which contains the JS bindings generated from
 *     `suggest.udl` by UniFFI. The classes defined in `RustSuggest.sys.mjs` are
 *     what we consume here in this file. If you have a question about the JS
 *     interface to the Rust component, try checking `RustSuggest.sys.mjs`, but
 *     as you get accustomed to UniFFI JS conventions you may find it simpler to
 *     refer directly to `suggest.udl`.
 * (4) `config.toml` [6], which defines which functions in the JS bindings are
 *     sync and which are async. Functions default to the "worker" thread, which
 *     means they are async. Some functions are "main", which means they are
 *     sync. Async functions return promises. This information is reflected in
 *     `RustSuggest.sys.mjs` of course: If a function is "worker", its JS
 *     binding will return a promise, and if it's "main" it won't.
 *
 * [1] https://github.com/mozilla/application-services/tree/main/components/suggest
 * [2] https://searchfox.org/mozilla-central/source/third_party/rust/suggest
 * [3] https://github.com/mozilla/application-services/blob/main/components/suggest/src/suggest.udl
 * [4] https://searchfox.org/mozilla-central/source/third_party/rust/suggest/src/suggest.udl
 * [5] https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js/components/generated/RustSuggest.sys.mjs
 * [6] https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js/config.toml
 */
export class SuggestBackendRust extends BaseFeature {
  get shouldEnable() {
    return lazy.UrlbarPrefs.get("quickSuggestRustEnabled");
  }

  /**
   * @returns {Promise}
   *   If ingest is pending this will be resolved when it's done. Otherwise it
   *   was resolved when the previous ingest finished.
   */
  get ingestPromise() {
    return this.#ingestPromise;
  }

  enable(enabled) {
    if (enabled) {
      this.#init();
    } else {
      this.#uninit();
    }
  }

  async query(searchString) {
    this.logger.info("Handling query: " + JSON.stringify(searchString));

    if (!this.#store) {
      // There must have been an error creating `#store`.
      this.logger.info("#store is null, returning");
      return [];
    }

    // Build the list of Rust providers to query. Each provider is identified by
    // an integer value defined on the `SuggestionProvider` object. Here we
    // convert the Rust suggestion types of our registered features to their
    // corresponding provider integer values.
    let providers = [];
    for (let [type, feature] of lazy.QuickSuggest
      .featuresByRustSuggestionType) {
      if (feature.isEnabled && feature.isRustSuggestionTypeEnabled(type)) {
        let key = type.toUpperCase();
        this.logger.debug("Adding provider to query:" + key);
        if (!lazy.SuggestionProvider.hasOwnProperty(key)) {
          this.logger.error(`SuggestionProvider["${key}"] is not defined!`);
          continue;
        }
        providers.push(lazy.SuggestionProvider[key]);
      }
    }

    let suggestions = await this.#store.query(
      new lazy.SuggestionQuery(searchString, providers)
    );

    for (let suggestion of suggestions) {
      let type = getSuggestionType(suggestion);
      if (!type) {
        continue;
      }

      suggestion.source = "rust";
      suggestion.provider = type;
      suggestion.is_sponsored = type == "Amp";
      if (Array.isArray(suggestion.icon)) {
        suggestion.icon_blob = new Blob([new Uint8Array(suggestion.icon)]);
        delete suggestion.icon;
      }
    }

    this.logger.debug(
      "Got suggestions: " + JSON.stringify(suggestions, null, 2)
    );

    return suggestions;
  }

  cancelQuery() {
    this.#store?.interrupt();
  }

  /**
   * nsITimerCallback
   */
  notify() {
    this.logger.info("Ingest timer fired");
    this.#ingest();
  }

  get #storePath() {
    return PathUtils.join(
      Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
      SUGGEST_STORE_BASENAME
    );
  }

  #init() {
    // Create the store.
    let path = this.#storePath;
    this.logger.info("Initializing SuggestStore: " + path);
    try {
      this.#store = lazy.SuggestStore.init(
        path,
        SuggestBackendRust._test_remoteSettingsConfig ??
          new lazy.RemoteSettingsConfig(
            lazy.Utils.SERVER_URL,
            lazy.Utils.actualBucketName("main"),
            "quicksuggest"
          )
      );
    } catch (error) {
      this.logger.error("Error initializing SuggestStore:");
      this.logger.error(error);
      return;
    }

    // Before registering the ingest timer, check the last-update pref, which is
    // created by the timer manager the first time we register it. If the pref
    // doesn't exist, this is the first time the Rust backend has been enabled
    // in this profile. In that case, perform ingestion immediately to make
    // automated and manual testing easier. Otherwise we'd need to wait at least
    // 30s (`app.update.timerFirstInterval`) for the timer manager to call us
    // back (and we'd also need to pass false for `skipFirst` below).
    let lastIngestSecs = Services.prefs.getIntPref(
      INGEST_TIMER_LAST_UPDATE_PREF,
      0
    );
    if (lastIngestSecs) {
      this.logger.info(
        `Last ingest: ${lastIngestSecs}s since epoch. Not ingesting now`
      );
    } else {
      this.logger.info("Last ingest time not found. Ingesting now");
      this.#ingest();
    }

    // Register the ingest timer.
    lazy.timerManager.registerTimer(
      INGEST_TIMER_ID,
      this,
      lazy.UrlbarPrefs.get("quicksuggest.rustIngestIntervalSeconds"),
      true // skipFirst
    );
  }

  #uninit() {
    this.#store = null;
    lazy.timerManager.unregisterTimer(INGEST_TIMER_ID);
  }

  async #ingest() {
    this.logger.info("Starting ingest");
    this.#ingestPromise = this.#store.ingest(
      new lazy.SuggestIngestionConstraints()
    );
    try {
      await this.#ingestPromise;
    } catch (error) {
      // Ingest can throw a `SuggestApiError` subclass called `Other` that has a
      // custom `reason` message, which is very helpful for diagnosing problems
      // with remote settings data in tests in particular.
      this.logger.error("Ingest error: " + (error.reason ?? error));
    }
    this.logger.info("Finished ingest");
  }

  async _test_ingest() {
    await this.#ingest();
  }

  // The `SuggestStore` instance.
  #store;

  #ingestPromise;
}

/**
 * Returns the type of a suggestion.
 *
 * @param {Suggestion} suggestion
 *   A suggestion object, an instance of one of the `Suggestion` subclasses.
 * @returns {string}
 *   The suggestion's type, e.g., "Amp", "Wikipedia", etc.
 */
function getSuggestionType(suggestion) {
  // Suggestion objects served by the Rust component don't have any inherent
  // type information other than the classes they are instances of. There's no
  // `type` property, for example. There's a base `Suggestion` class and many
  // `Suggestion` subclasses, one per type of suggestion. Each suggestion object
  // is an instance of one of these subclasses. We derive a suggestion's type
  // from the subclass it's an instance of.
  //
  // Unfortunately the subclasses are all anonymous, which means
  // `suggestion.constructor.name` is always an empty string. (This is due to
  // how UniFFI generates JS bindings.) Instead, the subclasses are defined as
  // properties on the base `Suggestion` class. For example,
  // `Suggestion.Wikipedia` is the (anonymous) Wikipedia suggestion class. To
  // find a suggestion's subclass, we loop through the keys on `Suggestion`
  // until we find the value the suggestion is an instance of. To avoid doing
  // this every time, we cache the mapping from suggestion constructor to key
  // the first time we encounter a new suggestion subclass.
  let type = gSuggestionTypesByCtor.get(suggestion.constructor);
  if (!type) {
    type = Object.keys(lazy.Suggestion).find(
      key => suggestion instanceof lazy.Suggestion[key]
    );
    if (type) {
      gSuggestionTypesByCtor.set(suggestion.constructor, type);
    } else {
      this.logger.error(
        "Unexpected error: Suggestion class not found on `Suggestion`. " +
          "Did the Rust component or its JS bindings change? " +
          "The suggestion is: " +
          JSON.stringify(suggestion)
      );
    }
  }
  return type;
}
