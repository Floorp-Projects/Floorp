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
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  TOP_SITES_MAX_SITES_PER_ROW: "resource://activity-stream/common/Reducers.jsm",
  TOP_SITES_DEFAULT_ROWS: "resource://activity-stream/common/Reducers.jsm",
});

// The scalar category of TopSites impression for Contextual Services
const SCALAR_CATEGORY_TOPSITES = "contextual.services.topsites.impression";

/**
 * This module exports a provider returning the user's newtab Top Sites.
 */

/**
 * A provider that returns the Top Sites shown on about:newtab.
 */
class ProviderTopSites extends UrlbarProvider {
  constructor() {
    super();
  }

  get PRIORITY() {
    // Top sites are prioritized over the UrlbarProviderPlaces provider.
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
    return (
      !queryContext.restrictSource &&
      !queryContext.searchString &&
      !queryContext.searchMode
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
    // If system.topsites is disabled, we would get stale or empty Top Sites
    // data. We check this condition here instead of in isActive because we
    // still want this provider to be restricting even if this is not true. If
    // it wasn't restricting, we would show the results from UrlbarProviderPlaces's
    // empty search behaviour. We aren't interested in those since they are very
    // similar to Top Sites and thus might be confusing, especially since users
    // can configure Top Sites but cannot configure the default empty search
    // results. See bug 1623666.
    if (
      !UrlbarPrefs.get("suggest.topsites") ||
      !Services.prefs.getBoolPref(
        "browser.newtabpage.activity-stream.feeds.system.topsites",
        false
      )
    ) {
      return;
    }

    let sites = AboutNewTab.getTopSites();

    let instance = this.queryInstance;

    // Filter out empty values. Site is empty when there's a gap between tiles
    // on about:newtab.
    sites = sites.filter(site => site);

    if (!UrlbarPrefs.get("sponsoredTopSites")) {
      sites = sites.filter(site => !site.sponsored_position);
    }

    // This is done here, rather than in the global scope, because
    // TOP_SITES_DEFAULT_ROWS causes the import of Reducers.jsm, and we want to
    // do that only when actually querying for Top Sites.
    if (this.topSitesRows === undefined) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "topSitesRows",
        "browser.newtabpage.activity-stream.topSitesRows",
        TOP_SITES_DEFAULT_ROWS
      );
    }

    // We usually respect maxRichResults, though we never show a number of Top
    // Sites greater than what is visible in the New Tab Page, because the
    // additional ones couldn't be managed from the page.
    let numTopSites = Math.min(
      UrlbarPrefs.get("maxRichResults"),
      TOP_SITES_MAX_SITES_PER_ROW * this.topSitesRows
    );
    sites = sites.slice(0, numTopSites);

    let sponsoredSites = [];
    let index = 1;
    sites = sites.map(link => {
      let site = {
        type: link.searchTopSite ? "search" : "url",
        url: link.url_urlbar || link.url,
        isPinned: !!link.isPinned,
        isSponsored: !!link.sponsored_position,
        // The newtab page allows the user to set custom site titles, which
        // are stored in `label`, so prefer it.  Search top sites currently
        // don't have titles but `hostname` instead.
        title: link.label || link.title || link.hostname || "",
        favicon: link.smallFavicon || link.favicon || undefined,
        sendAttributionRequest: !!link.sendAttributionRequest,
      };
      if (site.isSponsored) {
        let {
          sponsored_tile_id,
          sponsored_impression_url,
          sponsored_click_url,
        } = link;
        site = {
          ...site,
          sponsoredTileId: sponsored_tile_id,
          sponsoredImpressionUrl: sponsored_impression_url,
          sponsoredClickUrl: sponsored_click_url,
          position: index,
        };
        sponsoredSites.push(site);
      }
      index++;
      return site;
    });

    // Store Sponsored Top Sites so we can use it in `onEngagement`
    if (sponsoredSites.length) {
      this.sponsoredSites = sponsoredSites;
    }

    for (let site of sites) {
      switch (site.type) {
        case "url": {
          let payload = {
            title: site.title,
            url: site.url,
            icon: site.favicon,
            isPinned: site.isPinned,
            isSponsored: site.isSponsored,
            sendAttributionRequest: site.sendAttributionRequest,
          };
          if (site.isSponsored) {
            payload = {
              ...payload,
              sponsoredTileId: site.sponsoredTileId,
              sponsoredClickUrl: site.sponsoredClickUrl,
            };
          }
          let result = new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.URL,
            UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
            ...UrlbarResult.payloadAndSimpleHighlights(
              queryContext.tokens,
              payload
            )
          );

          let tabs;
          if (UrlbarPrefs.get("suggest.openpage")) {
            tabs = UrlbarProviderOpenTabs.getOpenTabs(
              queryContext.userContextId || 0,
              queryContext.isPrivate
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
          if (instance != this.queryInstance) {
            break;
          }

          addCallback(this, result);
          break;
        }
        case "search": {
          let engine = await UrlbarSearchUtils.engineForAlias(site.title);

          if (!engine && site.url) {
            // Look up the engine by its domain.
            let host;
            try {
              host = new URL(site.url).hostname;
            } catch (err) {}
            if (host) {
              engine = (
                await UrlbarSearchUtils.enginesForDomainPrefix(host)
              )[0];
            }
          }

          if (!engine) {
            // No engine found. We skip this Top Site.
            break;
          }

          if (instance != this.queryInstance) {
            break;
          }

          let result = new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              title: site.title,
              keyword: site.title,
              providesSearchMode: true,
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
          this.logger.error(`Unknown Top Site type: ${site.type}`);
          break;
      }
    }
  }

  /**
   * Called when the user starts and ends an engagement with the urlbar. We send
   * the impression ping for the sponsored TopSites, the impression scalar is
   * recorded as well.
   *
   * Note:
   *   * No telemetry recording in private browsing mode
   *   * The impression is only recorded for the "engagement" and "abandonment"
   *     states
   *
   * @param {boolean} isPrivate True if the engagement is in a private context.
   * @param {string} state The state of the engagement, one of: start,
   *        engagement, abandonment, discard.
   */
  onEngagement(isPrivate, state) {
    if (
      !isPrivate &&
      this.sponsoredSites &&
      ["engagement", "abandonment"].includes(state)
    ) {
      for (let site of this.sponsoredSites) {
        Services.telemetry.keyedScalarAdd(
          SCALAR_CATEGORY_TOPSITES,
          `urlbar_${site.position}`,
          1
        );
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            source: "urlbar",
            tile_id: site.sponsoredTileId || -1,
            position: site.position,
            reporting_url: site.sponsoredImpressionUrl,
            advertiser: site.title.toLocaleLowerCase(),
          },
          CONTEXTUAL_SERVICES_PING_TYPES.TOPSITES_IMPRESSION
        );
      }
    }

    this.sponsoredSites = null;
  }
}

var UrlbarProviderTopSites = new ProviderTopSites();
