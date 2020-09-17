/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Search service utilities for urlbar.  The only reason these functions aren't
 * a part of UrlbarUtils is that we want O(1) case-insensitive lookup for search
 * aliases, and to do that we need to observe the search service, persistent
 * state, and an init method.  A separate object is easier.
 */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarSearchUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
   * Gets the engine whose domain matches a given prefix.
   *
   * @param {string} prefix
   *   String containing the first part of the matching domain name.
   * @returns {nsISearchEngine}
   *   The matching engine or null if there isn't one.
   */
  async engineForDomainPrefix(prefix) {
    await this.init();
    for (let engine of await Services.search.getVisibleEngines()) {
      let domain = engine.getResultDomain();
      if (domain.startsWith(prefix) || domain.startsWith("www." + prefix)) {
        return engine;
      }
    }
    return null;
  }

  /**
   * Gets the engine with a given alias.
   *
   * @param {string} alias
   *   A search engine alias.  The alias string comparison is case insensitive.
   * @returns {nsISearchEngine}
   *   The matching engine or null if there isn't one.
   */
  async engineForAlias(alias) {
    await Promise.all([this.init(), this._refreshEnginesByAliasPromise]);
    return this._enginesByAlias.get(alias.toLocaleLowerCase()) || null;
  }

  /**
   * The list of engines with token ("@") aliases.
   *
   * @returns {array}
   *   Array of objects { engine, tokenAliases } for token alias engines.
   */
  async tokenAliasEngines() {
    await this.init();
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

  getDefaultEngine(isPrivate = false) {
    return this.separatePrivateDefaultUIEnabled &&
      this.separatePrivateDefault &&
      isPrivate
      ? Services.search.defaultPrivateEngine
      : Services.search.defaultEngine;
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
   * Gets the aliases of an engine.  For the user's convenience, we recognize
   * token versions of all non-token aliases.  For example, if the user has an
   * alias of "foo", then we recognize both "foo" and "@foo" as aliases for
   * foo's engine.  The returned list is therefore a superset of
   * `engine.aliases`.  Additionally, the returned aliases will be lower-cased
   * to make lookups and comparisons easier.
   *
   * @param {nsISearchEngine} engine
   *   The aliases of this search engine will be returned.
   * @returns {array}
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

var UrlbarSearchUtils = new SearchUtils();
