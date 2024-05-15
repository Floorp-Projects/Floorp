/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider returning the user's recent searches.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

// These prefs are relative to the `browser.urlbar` branch.
const ENABLED_PREF = "recentsearches.featureGate";
const SUGGEST_PREF = "suggest.recentsearches";
const EXPIRATION_PREF = "recentsearches.expirationMs";
const LASTDEFAULTCHANGED_PREF = "recentsearches.lastDefaultChanged";

/**
 * A provider that returns the Recent Searches performed by the user.
 */
class ProviderRecentSearches extends UrlbarProvider {
  constructor(...args) {
    super(...args);
    Services.obs.addObserver(this, lazy.SearchUtils.TOPIC_ENGINE_MODIFIED);
  }

  get name() {
    return "RecentSearches";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  isActive(queryContext) {
    return (
      lazy.UrlbarPrefs.get(ENABLED_PREF) &&
      lazy.UrlbarPrefs.get(SUGGEST_PREF) &&
      !queryContext.restrictSource &&
      !queryContext.searchString &&
      !queryContext.searchMode
    );
  }

  /**
   * We use the same priority as `UrlbarProviderTopSites` as these are both
   * shown on an empty urlbar query.
   *
   * @returns {number} The provider's priority for the given query.
   */
  getPriority() {
    return 1;
  }

  onEngagement(queryContext, controller, details) {
    let { result } = details;
    let engine = lazy.UrlbarSearchUtils.getDefaultEngine(
      queryContext.isPrivate
    );

    if (details.selType == "dismiss" && queryContext.formHistoryName) {
      lazy.FormHistory.update({
        op: "remove",
        fieldname: "searchbar-history",
        value: result.payload.suggestion,
        source: engine.name,
      }).catch(error =>
        console.error(`Removing form history failed: ${error}`)
      );
      controller.removeResult(result);
    }
  }

  async startQuery(queryContext, addCallback) {
    let engine = lazy.UrlbarSearchUtils.getDefaultEngine(
      queryContext.isPrivate
    );
    let results = await lazy.FormHistory.search(["value", "lastUsed"], {
      fieldname: "searchbar-history",
      source: engine.name,
    });

    let expiration = parseInt(lazy.UrlbarPrefs.get(EXPIRATION_PREF), 10);
    let lastDefaultChanged = parseInt(
      lazy.UrlbarPrefs.get(LASTDEFAULTCHANGED_PREF),
      10
    );
    let now = Date.now();

    // We only want to show searches since the last engine change, if we
    // havent changed the engine we expire the display of the searches
    // after a period of time.
    if (lastDefaultChanged != -1) {
      expiration = Math.min(expiration, now - lastDefaultChanged);
    }

    results = results.filter(
      result => now - Math.floor(result.lastUsed / 1000) < expiration
    );
    results.sort((a, b) => b.lastUsed - a.lastUsed);

    if (results.length > lazy.UrlbarPrefs.get("recentsearches.maxResults")) {
      results.length = lazy.UrlbarPrefs.get("recentsearches.maxResults");
    }

    for (let result of results) {
      let res = new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          engine: engine.name,
          suggestion: result.value,
          isBlockable: true,
          blockL10n: { id: "urlbar-result-menu-remove-from-history" },
          helpUrl:
            Services.urlFormatter.formatURLPref("app.support.baseURL") +
            "awesome-bar-result-menu",
        }
      );
      addCallback(this, res);
    }
  }

  observe(subject, topic, data) {
    switch (data) {
      case lazy.SearchUtils.MODIFIED_TYPE.DEFAULT:
        lazy.UrlbarPrefs.set(LASTDEFAULTCHANGED_PREF, Date.now().toString());
        break;
    }
  }
}

export var UrlbarProviderRecentSearches = new ProviderRecentSearches();
