/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Search service utilities for urlbar.  The only reason these functions aren't
 * a part of UrlbarUtils is that we want O(1) case-insensitive lookup for search
 * aliases, and to do that we need to observe the search service, persistent
 * state, and an init method.  A separate object is easier.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

/**
 * Search service utilities for urlbar.
 */
class SearchUtils {
  constructor() {
    this._refreshEnginesByAliasPromise = Promise.resolve();
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "separatePrivateDefaultUIEnabled",
      "browser.search.separatePrivateDefault.ui.enabled",
      false
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "separatePrivateDefault",
      "browser.search.separatePrivateDefault",
      false
    );
  }

  /**
   * Initializes the instance and also Services.search.
   */
  async init() {
    if (!this._initPromise) {
      this._initPromise = this._initInternal();
    }
    await this._initPromise;
  }

  /**
   * Gets the engines whose domains match a given prefix.
   *
   * @param {string} prefix
   *   String containing the first part of the matching domain name(s).
   * @param {object} [options]
   *   Options object.
   * @param {boolean} [options.matchAllDomainLevels]
   *   Match at each sub domain, for example "a.b.c.com" will be matched at
   *   "a.b.c.com", "b.c.com", and "c.com". Partial matches are always returned
   *   after perfect matches.
   * @param {boolean} [options.onlyEnabled]
   *   Match only engines that have not been disabled on the Search Preferences
   *   list.
   * @returns {Array<nsISearchEngine>}
   *   An array of all matching engines. An empty array if there are none.
   */
  async enginesForDomainPrefix(
    prefix,
    { matchAllDomainLevels = false, onlyEnabled = false } = {}
  ) {
    try {
      await this.init();
    } catch {
      return [];
    }
    prefix = prefix.toLowerCase();

    // Array of partially matched engines, added through matchPrefix().
    let partialMatchEngines = [];
    function matchPrefix(engine, engineHost) {
      let parts = engineHost.split(".");
      for (let i = 1; i < parts.length - 1; ++i) {
        if (parts.slice(i).join(".").startsWith(prefix)) {
          partialMatchEngines.push(engine);
        }
      }
    }

    // Array of perfectly matched engines. We also keep a Set for O(1) lookup.
    let perfectMatchEngines = [];
    let perfectMatchEngineSet = new Set();
    for (let engine of await Services.search.getVisibleEngines()) {
      if (engine.hideOneOffButton) {
        continue;
      }
      let domain = engine.searchUrlDomain;
      if (domain.startsWith(prefix) || domain.startsWith("www." + prefix)) {
        perfectMatchEngines.push(engine);
        perfectMatchEngineSet.add(engine);
      }

      if (matchAllDomainLevels) {
        // The prefix may or may not contain part of the public suffix. If
        // it contains a dot, we must match with and without the public suffix,
        // otherwise it's sufficient to just match without it.
        if (prefix.includes(".")) {
          matchPrefix(engine, domain);
        }
        matchPrefix(
          engine,
          domain.substr(0, domain.length - engine.searchUrlPublicSuffix.length)
        );
      }
    }

    // Build the final list of matching engines. Partial matches come after
    // perfect matches. Partial matches may be included in the perfect matches
    // list, so be careful not to include the same engine more than once.
    let engines = perfectMatchEngines;
    let engineSet = perfectMatchEngineSet;
    for (let engine of partialMatchEngines) {
      if (!engineSet.has(engine)) {
        engineSet.add(engine);
        engines.push(engine);
      }
    }
    return engines;
  }

  /**
   * Gets the engine with a given alias.
   *
   * @param {string} alias
   *   A search engine alias.  The alias string comparison is case insensitive.
   * @param {string} [searchString]
   *   Optional. If provided, we also enforce that there must be a space after
   *   the alias in the search string.
   * @returns {nsISearchEngine}
   *   The matching engine or null if there isn't one.
   */
  async engineForAlias(alias, searchString = null) {
    try {
      await Promise.all([this.init(), this._refreshEnginesByAliasPromise]);
    } catch {
      return null;
    }

    let engine = this._enginesByAlias.get(alias.toLocaleLowerCase());
    if (engine && searchString) {
      let query = lazy.UrlbarUtils.substringAfter(searchString, alias);
      // Match an alias only when it has a space after it.  If there's no trailing
      // space, then continue to treat it as part of the search string.
      if (!lazy.UrlbarTokenizer.REGEXP_SPACES_START.test(query)) {
        return null;
      }
    }
    return engine || null;
  }

  /**
   * The list of engines with token ("@") aliases.
   *
   * @returns {Array}
   *   Array of objects { engine, tokenAliases } for token alias engines or
   *   null if SearchService has not initialized.
   */
  async tokenAliasEngines() {
    try {
      await this.init();
    } catch {
      return [];
    }

    let tokenAliasEngines = [];
    for (let engine of await Services.search.getVisibleEngines()) {
      let tokenAliases = this._aliasesForEngine(engine).filter(a =>
        a.startsWith("@")
      );
      if (tokenAliases.length) {
        tokenAliasEngines.push({ engine, tokenAliases });
      }
    }
    return tokenAliasEngines;
  }

  /**
   * @param {nsISearchEngine} engine
   *   The engine to get the root domain of
   * @returns {string}
   *   The root domain of a search engine. e.g. If `engine` has the domain
   *   www.subdomain.rootdomain.com, `rootdomain` is returned. Returns the
   *   engine's domain if the engine's URL does not have a valid TLD.
   */
  getRootDomainFromEngine(engine) {
    let domain = engine.searchUrlDomain;
    let suffix = engine.searchUrlPublicSuffix;
    if (!suffix) {
      if (domain.endsWith(".test")) {
        suffix = "test";
      } else {
        return domain;
      }
    }
    domain = domain.substr(
      0,
      // -1 to remove the trailing dot.
      domain.length - suffix.length - 1
    );
    let domainParts = domain.split(".");
    return domainParts.pop();
  }

  /**
   * @param {boolean} [isPrivate]
   *   True if in a private context.
   * @returns {nsISearchEngine}
   *   The default engine or null if SearchService has not initialized.
   */
  getDefaultEngine(isPrivate = false) {
    if (!Services.search.hasSuccessfullyInitialized) {
      return null;
    }

    return this.separatePrivateDefaultUIEnabled &&
      this.separatePrivateDefault &&
      isPrivate
      ? Services.search.defaultPrivateEngine
      : Services.search.defaultEngine;
  }

  /**
   * To make analysis easier, we sanitize some engine names when
   * recording telemetry about search mode. This function returns the sanitized
   * key name to record in telemetry.
   *
   * @param {object} searchMode
   *   A search mode object. See UrlbarInput.setSearchMode.
   * @returns {string}
   *   A sanitized scalar key, used to access Telemetry data.
   */
  getSearchModeScalarKey(searchMode) {
    let scalarKey;
    if (searchMode.engineName) {
      let engine = Services.search.getEngineByName(searchMode.engineName);
      let resultDomain = engine.searchUrlDomain;
      // For built-in engines, sanitize the data in a few special cases to make
      // analysis easier.
      if (!engine.isAppProvided) {
        scalarKey = "other";
      } else if (resultDomain.includes("amazon.")) {
        // Group all the localized Amazon sites together.
        scalarKey = "Amazon";
      } else if (resultDomain.endsWith("wikipedia.org")) {
        // Group all the localized Wikipedia sites together.
        scalarKey = "Wikipedia";
      } else {
        scalarKey = searchMode.engineName;
      }
    } else if (searchMode.source) {
      scalarKey =
        lazy.UrlbarUtils.getResultSourceName(searchMode.source) || "other";
    }

    return scalarKey;
  }

  async _initInternal() {
    await Services.search.init();
    await this._refreshEnginesByAlias();
    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC, true);
  }

  async _refreshEnginesByAlias() {
    // See the comment at the top of this file.  The only reason we need this
    // class is for O(1) case-insensitive lookup for search aliases, which is
    // facilitated by _enginesByAlias.
    this._enginesByAlias = new Map();
    for (let engine of await Services.search.getVisibleEngines()) {
      if (!engine.hidden) {
        for (let alias of this._aliasesForEngine(engine)) {
          this._enginesByAlias.set(alias, engine);
        }
      }
    }
  }

  /**
   * Checks if the given uri is constructed by the default search engine.
   * When passing URI's to check against, it's best to use the "original" URI
   * that was requested, as the server may have redirected the request.
   *
   * @param {nsIURI | string} uri
   *   The uri to check.
   * @returns {string}
   *   The search terms used.
   *   Will return an empty string if it's not a default SERP, the string
   *   exceeds the maximum characters, or the default engine hasn't been
   *   initialized.
   */
  getSearchTermIfDefaultSerpUri(uri) {
    if (!Services.search.hasSuccessfullyInitialized || !uri) {
      return "";
    }

    // Creating a URI can throw.
    try {
      if (typeof uri == "string") {
        uri = Services.io.newURI(uri);
      }
    } catch (e) {
      return "";
    }

    let searchTerm = Services.search.defaultEngine.searchTermFromResult(uri);

    if (!searchTerm || searchTerm.length > lazy.UrlbarUtils.MAX_TEXT_LENGTH) {
      return "";
    }

    let searchTermWithSpacesRemoved = searchTerm.replaceAll(/\s/g, "");

    if (
      searchTermWithSpacesRemoved.startsWith("https://") ||
      searchTermWithSpacesRemoved.startsWith("http://")
    ) {
      return "";
    }

    try {
      let info = Services.uriFixup.getFixupURIInfo(
        searchTermWithSpacesRemoved,
        Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
          Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
      );
      if (info.keywordAsSent) {
        return searchTerm;
      }
    } catch (e) {}
    return "";
  }

  /**
   * Compares the query parameters of two SERPs to see if one is equivalent to
   * the other. URL `x` is equivalent to URL `y` if
   *   (a) `y` contains at least all the query parameters contained in `x`, and
   *   (b) The values of the query parameters contained in both `x` and `y `are
   *       the same.
   *
   * This function does not compare the SERPs' origins or pathnames.
   * `historySerp` can have a different origin and/or pathname than
   * `generatedSerp` and still be considered equivalent.
   *
   * @param {string} historySerp
   *   The SERP from history whose params should be contained in
   *   `generatedSerp`.
   * @param {string} generatedSerp
   *   The search URL we would generate for a search result with the same search
   *   string used in `historySerp`.
   * @param {Array} [ignoreParams]
   *   A list of params to ignore in the matching, i.e. params that can be
   *   contained in `historySerp` but not be in `generatedSerp`.
   * @returns {boolean} True if `historySerp` can be deduped by `generatedSerp`.
   */
  serpsAreEquivalent(historySerp, generatedSerp, ignoreParams = []) {
    let historyParams = new URL(historySerp).searchParams;
    let generatedParams = new URL(generatedSerp).searchParams;
    if (
      !Array.from(historyParams.entries()).every(
        ([key, value]) =>
          ignoreParams.includes(key) || value === generatedParams.get(key)
      )
    ) {
      return false;
    }

    return true;
  }

  /**
   * Gets the aliases of an engine.  For the user's convenience, we recognize
   * token versions of all non-token aliases.  For example, if the user has an
   * alias of "foo", then we recognize both "foo" and "@foo" as aliases for
   * foo's engine.  The returned list is therefore a superset of
   * `engine.aliases`.  Additionally, the returned aliases will be lower-cased
   * to make lookups and comparisons easier.
   *
   * @param {nsISearchEngine} engine
   *   The aliases of this search engine will be returned.
   * @returns {Array}
   *   An array of lower-cased string aliases as described above.
   */
  _aliasesForEngine(engine) {
    return engine.aliases.reduce((aliases, aliasWithCase) => {
      // We store lower-cased aliases to make lookups and comparisons easier.
      let alias = aliasWithCase.toLocaleLowerCase();
      aliases.push(alias);
      if (!alias.startsWith("@")) {
        aliases.push("@" + alias);
      }
      return aliases;
    }, []);
  }

  /**
   * @param {string} engineName
   *   Name of the search engine.
   * @returns {nsISearchEngine}
   *   The engine based on engineName or null if SearchService has not
   *   initialized.
   */
  getEngineByName(engineName) {
    if (!Services.search.hasSuccessfullyInitialized) {
      return null;
    }

    return Services.search.getEngineByName(engineName);
  }

  observe(subject, topic, data) {
    switch (data) {
      case "engine-added":
      case "engine-changed":
      case "engine-removed":
      case "engine-default":
        this._refreshEnginesByAliasPromise = this._refreshEnginesByAlias();
        break;
    }
  }
}

export var UrlbarSearchUtils = new SearchUtils();
