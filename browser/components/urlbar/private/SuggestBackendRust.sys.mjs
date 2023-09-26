/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SuggestIngestionConstraints: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestStore: "resource://gre/modules/RustSuggest.sys.mjs",
  Suggestion: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionQuery: "resource://gre/modules/RustSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

const SUGGEST_STORE_BASENAME = "suggest.sqlite";

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
    return (
      lazy.UrlbarPrefs.get("quickSuggestRustEnabled") &&
      lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled")
    );
  }

  async enable(enabled) {
    if (!enabled) {
      this.#store = null;
    } else {
      let store = await this.#initStore(this.#storePath);
      if (this.isEnabled) {
        this.#store = store;
      }
    }
  }

  async query(searchString) {
    this.logger.info("Handling query: " + JSON.stringify(searchString));

    // Store initialization is async, so even when this feature is enabled,
    // `#store` may still be null.
    if (!this.#store) {
      this.logger.info("Store not yet initialized, returning");
      return [];
    }

    searchString = searchString.toLocaleLowerCase();

    let suggestions = await this.#store.query(
      new lazy.SuggestionQuery(
        searchString,
        lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored"),
        lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
      )
    );

    for (let suggestion of suggestions) {
      let type = getSuggestionType(suggestion);
      if (!type) {
        continue;
      }

      suggestion.source = "rust";
      suggestion.provider = type;
      suggestion.is_sponsored = type == "Amp";

      // TODO: Handle icons.
      suggestion.icon = null;
    }

    this.logger.debug(
      "Got suggestions: " + JSON.stringify(suggestions, null, 2)
    );

    return suggestions;
  }

  cancelQuery() {
    this.#store?.interrupt();
  }

  get #storePath() {
    return PathUtils.join(
      Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
      SUGGEST_STORE_BASENAME
    );
  }

  async #initStore(path) {
    let store;
    this.logger.info("Initializing SuggestStore: " + path);
    try {
      store = lazy.SuggestStore.init(path);
    } catch (error) {
      this.logger.error("Error initializing SuggestStore:");
      this.logger.error(error);
      return null;
    }

    // TODO: (1) Don't ingest if the last ingest was recent enough, (2)
    // periodically ingest. Basically, emulate the desktop remote settings
    // client.
    this.logger.info("Ingesting SuggestStore");
    try {
      await store.ingest(new lazy.SuggestIngestionConstraints());
    } catch (error) {
      this.logger.error("Error ingesting SuggestStore:");
      this.logger.error(error);
      return null;
    }

    return store;
  }

  // The `SuggestStore` instance.
  #store;
}

/**
 * Returns the type of a suggestion.
 *
 * @param {Suggestion} suggestion
 *   An suggestion object, an instance of one of the `Suggestion` subclasses.
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
