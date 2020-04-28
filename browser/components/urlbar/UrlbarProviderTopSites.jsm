/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderTopSites"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.TopSites")
);

/**
 * This module exports a provider returning the user's newtab Top Sites.
 */

/**
 * A provider that returns the Top Sites shown on about:newtab.
 */
class ProviderTopSites extends UrlbarProvider {
  constructor() {
    super();
    // Maps the running queries by queryContext.
    this.queries = new Map();
  }

  get PRIORITY() {
    // Top sites are prioritized over the UnifiedComplete provider.
    return 1;
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   * Not using a unique name will cause the newest registration to win.
   */
  get name() {
    return "UrlbarProviderTopSites";
  }

  /**
   * The type of the provider.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // If top sites on new tab are disabled, the pref below will be false, and
    // activity stream's top sites will be unavailable (an empty array), so we
    // make this provider inactive.  For empty search strings, we instead show
    // the most frecent URLs in the user's history from the UnifiedComplete
    // provider.
    return (
      UrlbarPrefs.get("openViewOnFocus") &&
      !queryContext.searchString &&
      Services.prefs.getBoolPref(
        "browser.newtabpage.activity-stream.feeds.topsites",
        false
      )
    );
  }

  /**
   * Gets the provider's priority.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return this.PRIORITY;
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    let sites = AboutNewTab.getTopSites();

    let instance = {};
    this.queries.set(queryContext, instance);

    // Filter out empty values. Site is empty when there's a gap between tiles
    // on about:newtab.
    sites = sites.filter(site => site);
    // We want the top 8 sites.
    sites = sites.slice(0, 8);

    sites = sites.map(link => ({
      type: link.searchTopSite ? "search" : "url",
      url: link.url,
      isPinned: link.isPinned,
      // The newtab page allows the user to set custom site titles, which
      // are stored in `label`, so prefer it.  Search top sites currently
      // don't have titles but `hostname` instead.
      title: link.label || link.title || link.hostname || "",
      favicon: link.smallFavicon || link.favicon || null,
    }));

    for (let site of sites) {
      switch (site.type) {
        case "url": {
          let result = new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.URL,
            UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              title: site.title,
              url: site.url,
              icon: site.favicon,
              isPinned: site.isPinned,
            })
          );

          let tabs;
          if (UrlbarPrefs.get("suggest.openpage")) {
            tabs = UrlbarProviderOpenTabs.openTabs.get(
              queryContext.userContextId || 0
            );
          }

          if (tabs && tabs.includes(site.url.replace(/#.*$/, ""))) {
            result.type = UrlbarUtils.RESULT_TYPE.TAB_SWITCH;
            result.source = UrlbarUtils.RESULT_SOURCE.TABS;
          } else if (UrlbarPrefs.get("suggest.bookmark")) {
            let bookmark = await PlacesUtils.bookmarks.fetch({
              url: new URL(result.payload.url),
            });
            if (bookmark) {
              result.source = UrlbarUtils.RESULT_SOURCE.BOOKMARKS;
            }
          }

          // Our query has been cancelled.
          if (!this.queries.get(queryContext)) {
            break;
          }

          addCallback(this, result);
          break;
        }
        case "search": {
          let engine = await PlacesSearchAutocompleteProvider.engineForAlias(
            site.title
          );

          if (!engine && site.url) {
            // Look up the engine by its domain.
            let host;
            try {
              host = new URL(site.url).hostname;
            } catch (err) {}
            if (host) {
              engine = await PlacesSearchAutocompleteProvider.engineForDomainPrefix(
                host
              );
            }
          }

          if (!engine) {
            // No engine found. We skip this Top Site.
            break;
          }

          if (!this.queries.get(queryContext)) {
            break;
          }

          let result = new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              title: site.title,
              keyword: site.title,
              keywordOffer: UrlbarUtils.KEYWORD_OFFER.HIDE,
              engine: engine.name,
              query: "",
              icon: site.favicon,
              isPinned: site.isPinned,
            })
          );
          addCallback(this, result);
          break;
        }
        default:
          Cu.reportError(`Unknown Top Site type: ${site.type}`);
          break;
      }
    }
    this.queries.delete(queryContext);
  }

  /**
   * Cancels a running query,
   * @param {UrlbarQueryContext} queryContext the query context object to cancel
   *        query for.
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    this.queries.delete(queryContext);
  }
}

var UrlbarProviderTopSites = new ProviderTopSites();
