/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.sys.mjs"
);
const { TippyTopProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/TippyTopProvider.sys.mjs"
);
const { insertPinned, TOP_SITES_MAX_SITES_PER_ROW } =
  ChromeUtils.importESModule(
    "resource://activity-stream/common/Reducers.sys.mjs"
  );
const { Dedupe } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Dedupe.sys.mjs"
);
const { shortURL } = ChromeUtils.import(
  "resource://activity-stream/lib/ShortURL.jsm"
);
const { getDefaultOptions } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamStorage.jsm"
);
const {
  CUSTOM_SEARCH_SHORTCUTS,
  SEARCH_SHORTCUTS_EXPERIMENT,
  SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF,
  SEARCH_SHORTCUTS_HAVE_PINNED_PREF,
  checkHasSearchEngine,
  getSearchProvider,
  getSearchFormURL,
} = ChromeUtils.importESModule(
  "resource://activity-stream/lib/SearchShortcuts.sys.mjs"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "FilterAdult",
  "resource://activity-stream/lib/FilterAdult.jsm"
);
ChromeUtils.defineESModuleGetters(lazy, {
  LinksCache: "resource://activity-stream/lib/LinksCache.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PageThumbs: "resource://gre/modules/PageThumbs.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  Sampling: "resource://gre/modules/components-utils/Sampling.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm"
);

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("TopSitesFeed");
});

// `contextId` is a unique identifier used by Contextual Services
const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
XPCOMUtils.defineLazyGetter(lazy, "contextId", () => {
  let _contextId = Services.prefs.getStringPref(CONTEXT_ID_PREF, null);
  if (!_contextId) {
    _contextId = String(Services.uuid.generateUUID());
    Services.prefs.setStringPref(CONTEXT_ID_PREF, _contextId);
  }
  return _contextId;
});

const DEFAULT_SITES_PREF = "default.sites";
const SHOWN_ON_NEWTAB_PREF = "feeds.topsites";
const DEFAULT_TOP_SITES = [];
const FRECENCY_THRESHOLD = 100 + 1; // 1 visit (skip first-run/one-time pages)
const MIN_FAVICON_SIZE = 96;
const CACHED_LINK_PROPS_TO_MIGRATE = ["screenshot", "customScreenshot"];
const PINNED_FAVICON_PROPS_TO_MIGRATE = [
  "favicon",
  "faviconRef",
  "faviconSize",
];
const SECTION_ID = "topsites";
const ROWS_PREF = "topSitesRows";
const SHOW_SPONSORED_PREF = "showSponsoredTopSites";
// The default total number of sponsored top sites to fetch from Contile
// and Pocket.
const MAX_NUM_SPONSORED = 2;
// Nimbus variable for the total number of sponsored top sites including
// both Contile and Pocket sources.
// The default will be `MAX_NUM_SPONSORED` if this variable is unspecified.
const NIMBUS_VARIABLE_MAX_SPONSORED = "topSitesMaxSponsored";
// Nimbus variable to allow more than two sponsored tiles from Contile to be
//considered for Top Sites.
const NIMBUS_VARIABLE_ADDITIONAL_TILES =
  "topSitesUseAdditionalTilesFromContile";
// Nimbus variable to enable the SOV feature for sponsored tiles.
const NIMBUS_VARIABLE_CONTILE_SOV_ENABLED = "topSitesContileSovEnabled";
// Nimbu variable for the total number of sponsor topsite that come from Contile
// The default will be `CONTILE_MAX_NUM_SPONSORED` if variable is unspecified.
const NIMBUS_VARIABLE_CONTILE_MAX_NUM_SPONSORED = "topSitesContileMaxSponsored";

// Search experiment stuff
const FILTER_DEFAULT_SEARCH_PREF = "improvesearch.noDefaultSearchTile";
const SEARCH_FILTERS = [
  "google",
  "search.yahoo",
  "yahoo",
  "bing",
  "ask",
  "duckduckgo",
];

const REMOTE_SETTING_DEFAULTS_PREF = "browser.topsites.useRemoteSetting";
const DEFAULT_SITES_OVERRIDE_PREF =
  "browser.newtabpage.activity-stream.default.sites";
const DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH = "browser.topsites.experiment.";

// Mozilla Tiles Service (Contile) prefs
// Nimbus variable for the Contile integration. It falls back to the pref:
// `browser.topsites.contile.enabled`.
const NIMBUS_VARIABLE_CONTILE_ENABLED = "topSitesContileEnabled";
const CONTILE_ENDPOINT_PREF = "browser.topsites.contile.endpoint";
const CONTILE_UPDATE_INTERVAL = 15 * 60 * 1000; // 15 minutes
// The maximum number of sponsored top sites to fetch from Contile.
const CONTILE_MAX_NUM_SPONSORED = 2;
const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";
const CONTILE_CACHE_PREF = "browser.topsites.contile.cachedTiles";
const CONTILE_CACHE_VALID_FOR_PREF = "browser.topsites.contile.cacheValidFor";
const CONTILE_CACHE_LAST_FETCH_PREF = "browser.topsites.contile.lastFetch";

// Partners of sponsored tiles.
const SPONSORED_TILE_PARTNER_AMP = "amp";
const SPONSORED_TILE_PARTNER_MOZ_SALES = "moz-sales";
const SPONSORED_TILE_PARTNERS = new Set([
  SPONSORED_TILE_PARTNER_AMP,
  SPONSORED_TILE_PARTNER_MOZ_SALES,
]);

function getShortURLForCurrentSearch() {
  const url = shortURL({ url: Services.search.defaultEngine.searchForm });
  return url;
}

class ContileIntegration {
  constructor(topSitesFeed) {
    this._topSitesFeed = topSitesFeed;
    this._lastPeriodicUpdate = 0;
    this._sites = [];
    // The Share-of-Voice object managed by Shepherd and sent via Contile.
    this._sov = null;
  }

  get sites() {
    return this._sites;
  }

  get sov() {
    return this._sov;
  }

  periodicUpdate() {
    let now = Date.now();
    if (now - this._lastPeriodicUpdate >= CONTILE_UPDATE_INTERVAL) {
      this._lastPeriodicUpdate = now;
      this.refresh();
    }
  }

  async refresh() {
    let updateDefaultSites = await this._fetchSites();
    await this._topSitesFeed.allocatePositions();
    if (updateDefaultSites) {
      this._topSitesFeed._readDefaults();
    }
  }

  /**
   * Clear Contile Cache Prefs.
   */
  _resetContileCachePrefs() {
    Services.prefs.clearUserPref(CONTILE_CACHE_PREF);
    Services.prefs.clearUserPref(CONTILE_CACHE_LAST_FETCH_PREF);
    Services.prefs.clearUserPref(CONTILE_CACHE_VALID_FOR_PREF);
  }

  /**
   * Filter the tiles whose sponsor is on the Top Sites sponsor blocklist.
   *
   * @param {array} tiles
   *   An array of the tile objects
   */
  _filterBlockedSponsors(tiles) {
    const blocklist = JSON.parse(
      Services.prefs.getStringPref(TOP_SITES_BLOCKED_SPONSORS_PREF, "[]")
    );
    return tiles.filter(tile => !blocklist.includes(shortURL(tile)));
  }

  /**
   * Calculate the time Contile response is valid for based on cache-control header
   *
   * @param {string} cacheHeader
   *   string value of the Contile resposne cache-control header
   */
  _extractCacheValidFor(cacheHeader) {
    if (!cacheHeader) {
      lazy.log.warn("Contile response cache control header is empty");
      return 0;
    }
    const [, staleIfError] = cacheHeader.match(/stale-if-error=\s*([0-9]+)/i);
    const [, maxAge] = cacheHeader.match(/max-age=\s*([0-9]+)/i);
    const validFor =
      Number.parseInt(staleIfError, 10) + Number.parseInt(maxAge, 10);
    return isNaN(validFor) ? 0 : validFor;
  }

  /**
   * Load Tiles from Contile Cache Prefs
   */
  _loadTilesFromCache() {
    lazy.log.info("Contile client is trying to load tiles from local cache.");
    const now = Math.round(Date.now() / 1000);
    const lastFetch = Services.prefs.getIntPref(
      CONTILE_CACHE_LAST_FETCH_PREF,
      0
    );
    const validFor = Services.prefs.getIntPref(CONTILE_CACHE_VALID_FOR_PREF, 0);
    if (now <= lastFetch + validFor) {
      try {
        let cachedTiles = JSON.parse(
          Services.prefs.getStringPref(CONTILE_CACHE_PREF)
        );
        cachedTiles = this._filterBlockedSponsors(cachedTiles);
        this._sites = cachedTiles;
        lazy.log.info("Local cache loaded.");
        return true;
      } catch (error) {
        lazy.log.warn(`Failed to load tiles from local cache: ${error}.`);
        return false;
      }
    }

    return false;
  }

  /**
   * Determine number of Tiles to get from Contile
   */
  _getMaxNumFromContile() {
    return (
      lazy.NimbusFeatures.pocketNewtab.getVariable(
        NIMBUS_VARIABLE_CONTILE_MAX_NUM_SPONSORED
      ) ?? CONTILE_MAX_NUM_SPONSORED
    );
  }

  async _fetchSites() {
    if (
      !lazy.NimbusFeatures.newtab.getVariable(
        NIMBUS_VARIABLE_CONTILE_ENABLED
      ) ||
      !this._topSitesFeed.store.getState().Prefs.values[SHOW_SPONSORED_PREF]
    ) {
      if (this._sites.length) {
        this._sites = [];
        return true;
      }
      return false;
    }
    try {
      let url = Services.prefs.getStringPref(CONTILE_ENDPOINT_PREF);
      const response = await fetch(url, { credentials: "omit" });
      if (!response.ok) {
        lazy.log.warn(
          `Contile endpoint returned unexpected status: ${response.status}`
        );
        if (response.status === 304 || response.status >= 500) {
          return this._loadTilesFromCache();
        }
      }

      const lastFetch = Math.round(Date.now() / 1000);
      Services.prefs.setIntPref(CONTILE_CACHE_LAST_FETCH_PREF, lastFetch);

      // Contile returns 204 indicating there is no content at the moment.
      // If this happens, it will clear `this._sites` reset the cached tiles
      // to an empty array.
      if (response.status === 204) {
        if (this._sites.length) {
          this._sites = [];
          Services.prefs.setStringPref(
            CONTILE_CACHE_PREF,
            JSON.stringify(this._sites)
          );
          return true;
        }
        return false;
      }
      const body = await response.json();

      if (body?.sov) {
        this._sov = JSON.parse(atob(body.sov));
      }
      if (body?.tiles && Array.isArray(body.tiles)) {
        const useAdditionalTiles = lazy.NimbusFeatures.newtab.getVariable(
          NIMBUS_VARIABLE_ADDITIONAL_TILES
        );

        const maxNumFromContile = this._getMaxNumFromContile();

        let { tiles } = body;
        if (
          useAdditionalTiles !== undefined &&
          !useAdditionalTiles &&
          tiles.length > maxNumFromContile
        ) {
          tiles.length = maxNumFromContile;
        }
        tiles = this._filterBlockedSponsors(tiles);
        if (tiles.length > maxNumFromContile) {
          lazy.log.info("Remove unused links from Contile");
          tiles.length = maxNumFromContile;
        }
        this._sites = tiles;
        Services.prefs.setStringPref(
          CONTILE_CACHE_PREF,
          JSON.stringify(this._sites)
        );
        Services.prefs.setIntPref(
          CONTILE_CACHE_VALID_FOR_PREF,
          this._extractCacheValidFor(
            response.headers.get("cache-control") ||
              response.headers.get("Cache-Control")
          )
        );

        return true;
      }
    } catch (error) {
      lazy.log.warn(
        `Failed to fetch data from Contile server: ${error.message}`
      );
      return this._loadTilesFromCache();
    }
    return false;
  }
}

class TopSitesFeed {
  constructor() {
    this._contile = new ContileIntegration(this);
    this._tippyTopProvider = new TippyTopProvider();
    XPCOMUtils.defineLazyGetter(
      this,
      "_currentSearchHostname",
      getShortURLForCurrentSearch
    );
    this.dedupe = new Dedupe(this._dedupeKey);
    this.frecentCache = new lazy.LinksCache(
      lazy.NewTabUtils.activityStreamLinks,
      "getTopSites",
      CACHED_LINK_PROPS_TO_MIGRATE,
      (oldOptions, newOptions) =>
        // Refresh if no old options or requesting more items
        !(oldOptions.numItems >= newOptions.numItems)
    );
    this.pinnedCache = new lazy.LinksCache(
      lazy.NewTabUtils.pinnedLinks,
      "links",
      [...CACHED_LINK_PROPS_TO_MIGRATE, ...PINNED_FAVICON_PROPS_TO_MIGRATE]
    );
    lazy.PageThumbs.addExpirationFilter(this);
    this._nimbusChangeListener = this._nimbusChangeListener.bind(this);
  }

  _nimbusChangeListener(event, reason) {
    // The Nimbus API current doesn't specify the changed variable(s) in the
    // listener callback, so we have to refresh unconditionally on every change
    // of the `newtab` feature. It should be a manageable overhead given the
    // current update cadence (6 hours) of Nimbus.
    //
    // Skip the experiment and rollout loading reasons since this feature has
    // `isEarlyStartup` enabled, the feature variables are already available
    // before the experiment or rollout loads.
    if (
      !["feature-experiment-loaded", "feature-rollout-loaded"].includes(reason)
    ) {
      this._contile.refresh();
    }
  }

  init() {
    // If the feed was previously disabled PREFS_INITIAL_VALUES was never received
    this._readDefaults({ isStartup: true });
    this._storage = this.store.dbStorage.getDbTable("sectionPrefs");
    this._contile.refresh();
    Services.obs.addObserver(this, "browser-search-engine-modified");
    Services.obs.addObserver(this, "browser-region-updated");
    Services.prefs.addObserver(REMOTE_SETTING_DEFAULTS_PREF, this);
    Services.prefs.addObserver(DEFAULT_SITES_OVERRIDE_PREF, this);
    Services.prefs.addObserver(DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH, this);
    lazy.NimbusFeatures.newtab.onUpdate(this._nimbusChangeListener);
  }

  uninit() {
    lazy.PageThumbs.removeExpirationFilter(this);
    Services.obs.removeObserver(this, "browser-search-engine-modified");
    Services.obs.removeObserver(this, "browser-region-updated");
    Services.prefs.removeObserver(REMOTE_SETTING_DEFAULTS_PREF, this);
    Services.prefs.removeObserver(DEFAULT_SITES_OVERRIDE_PREF, this);
    Services.prefs.removeObserver(DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH, this);
    lazy.NimbusFeatures.newtab.offUpdate(this._nimbusChangeListener);
  }

  observe(subj, topic, data) {
    switch (topic) {
      case "browser-search-engine-modified":
        // We should update the current top sites if the search engine has been changed since
        // the search engine that gets filtered out of top sites has changed.
        // We also need to drop search shortcuts when their engine gets removed / hidden.
        if (
          data === "engine-default" &&
          this.store.getState().Prefs.values[FILTER_DEFAULT_SEARCH_PREF]
        ) {
          delete this._currentSearchHostname;
          this._currentSearchHostname = getShortURLForCurrentSearch();
        }
        this.refresh({ broadcast: true });
        break;
      case "browser-region-updated":
        this._readDefaults();
        break;
      case "nsPref:changed":
        if (
          data === REMOTE_SETTING_DEFAULTS_PREF ||
          data === DEFAULT_SITES_OVERRIDE_PREF ||
          data.startsWith(DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH)
        ) {
          this._readDefaults();
        }
        break;
    }
  }

  _dedupeKey(site) {
    return site && site.hostname;
  }

  /**
   * _readDefaults - sets DEFAULT_TOP_SITES
   */
  async _readDefaults({ isStartup = false } = {}) {
    this._useRemoteSetting = false;

    if (!Services.prefs.getBoolPref(REMOTE_SETTING_DEFAULTS_PREF)) {
      this.refreshDefaults(
        this.store.getState().Prefs.values[DEFAULT_SITES_PREF],
        { isStartup }
      );
      return;
    }

    // Try using default top sites from enterprise policies or tests. The pref
    // is locked when set via enterprise policy. Tests have no default sites
    // unless they set them via this pref.
    if (
      Services.prefs.prefIsLocked(DEFAULT_SITES_OVERRIDE_PREF) ||
      Cu.isInAutomation
    ) {
      let sites = Services.prefs.getStringPref(DEFAULT_SITES_OVERRIDE_PREF, "");
      this.refreshDefaults(sites, { isStartup });
      return;
    }

    // Clear out the array of any previous defaults.
    DEFAULT_TOP_SITES.length = 0;

    // Read defaults from contile.
    const contileEnabled = lazy.NimbusFeatures.newtab.getVariable(
      NIMBUS_VARIABLE_CONTILE_ENABLED
    );
    let hasContileTiles = false;
    if (contileEnabled) {
      let sponsoredPosition = 1;
      for (let site of this._contile.sites) {
        let hostname = shortURL(site);
        let link = {
          isDefault: true,
          url: site.url,
          hostname,
          sendAttributionRequest: false,
          label: site.name,
          show_sponsored_label: hostname !== "yandex",
          sponsored_position: sponsoredPosition++,
          sponsored_click_url: site.click_url,
          sponsored_impression_url: site.impression_url,
          sponsored_tile_id: site.id,
          partner: SPONSORED_TILE_PARTNER_AMP,
        };
        if (site.image_url && site.image_size >= MIN_FAVICON_SIZE) {
          // Only use the image from Contile if it's hi-res, otherwise, fallback
          // to the built-in favicons.
          link.favicon = site.image_url;
          link.faviconSize = site.image_size;
        }
        DEFAULT_TOP_SITES.push(link);
      }
      hasContileTiles = sponsoredPosition > 1;
    }

    // Read defaults from remote settings.
    this._useRemoteSetting = true;
    let remoteSettingData = await this._getRemoteConfig();

    const sponsoredBlocklist = JSON.parse(
      Services.prefs.getStringPref(TOP_SITES_BLOCKED_SPONSORS_PREF, "[]")
    );

    for (let siteData of remoteSettingData) {
      let hostname = shortURL(siteData);
      // Drop default sites when Contile already provided a sponsored one with
      // the same host name.
      if (
        contileEnabled &&
        DEFAULT_TOP_SITES.findIndex(site => site.hostname === hostname) > -1
      ) {
        continue;
      }
      // Also drop those sponsored sites that were blocked by the user before
      // with the same hostname.
      if (
        siteData.sponsored_position &&
        sponsoredBlocklist.includes(hostname)
      ) {
        continue;
      }
      let link = {
        isDefault: true,
        url: siteData.url,
        hostname,
        sendAttributionRequest: !!siteData.send_attribution_request,
      };
      if (siteData.url_urlbar_override) {
        link.url_urlbar = siteData.url_urlbar_override;
      }
      if (siteData.title) {
        link.label = siteData.title;
      }
      if (siteData.search_shortcut) {
        link = await this.topSiteToSearchTopSite(link);
      } else if (siteData.sponsored_position) {
        if (contileEnabled && hasContileTiles) {
          continue;
        }
        const {
          sponsored_position,
          sponsored_tile_id,
          sponsored_impression_url,
          sponsored_click_url,
        } = siteData;
        link = {
          sponsored_position,
          sponsored_tile_id,
          sponsored_impression_url,
          sponsored_click_url,
          show_sponsored_label: link.hostname !== "yandex",
          ...link,
        };
      }
      DEFAULT_TOP_SITES.push(link);
    }

    this.refresh({ broadcast: true, isStartup });
  }

  refreshDefaults(sites, { isStartup = false } = {}) {
    // Clear out the array of any previous defaults
    DEFAULT_TOP_SITES.length = 0;

    // Add default sites if any based on the pref
    if (sites) {
      for (const url of sites.split(",")) {
        const site = {
          isDefault: true,
          url,
        };
        site.hostname = shortURL(site);
        DEFAULT_TOP_SITES.push(site);
      }
    }

    this.refresh({ broadcast: true, isStartup });
  }

  async _getRemoteConfig(firstTime = true) {
    if (!this._remoteConfig) {
      this._remoteConfig = await lazy.RemoteSettings("top-sites");
      this._remoteConfig.on("sync", () => {
        this._readDefaults();
      });
    }

    let result = [];
    let failed = false;
    try {
      result = await this._remoteConfig.get();
    } catch (ex) {
      console.error(ex);
      failed = true;
    }
    if (!result.length) {
      console.error("Received empty top sites configuration!");
      failed = true;
    }
    // If we failed, or the result is empty, try loading from the local dump.
    if (firstTime && failed) {
      await this._remoteConfig.db.clear();
      // Now call this again.
      return this._getRemoteConfig(false);
    }

    // Sort sites based on the "order" attribute.
    result.sort((a, b) => a.order - b.order);

    result = result.filter(topsite => {
      // Filter by region.
      if (topsite.exclude_regions?.includes(lazy.Region.home)) {
        return false;
      }
      if (
        topsite.include_regions?.length &&
        !topsite.include_regions.includes(lazy.Region.home)
      ) {
        return false;
      }

      // Filter by locale.
      if (topsite.exclude_locales?.includes(Services.locale.appLocaleAsBCP47)) {
        return false;
      }
      if (
        topsite.include_locales?.length &&
        !topsite.include_locales.includes(Services.locale.appLocaleAsBCP47)
      ) {
        return false;
      }

      // Filter by experiment.
      // Exclude this top site if any of the specified experiments are running.
      if (
        topsite.exclude_experiments?.some(experimentID =>
          Services.prefs.getBoolPref(
            DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH + experimentID,
            false
          )
        )
      ) {
        return false;
      }
      // Exclude this top site if none of the specified experiments are running.
      if (
        topsite.include_experiments?.length &&
        topsite.include_experiments.every(
          experimentID =>
            !Services.prefs.getBoolPref(
              DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH + experimentID,
              false
            )
        )
      ) {
        return false;
      }

      return true;
    });

    return result;
  }

  filterForThumbnailExpiration(callback) {
    const { rows } = this.store.getState().TopSites;
    callback(
      rows.reduce((acc, site) => {
        acc.push(site.url);
        if (site.customScreenshotURL) {
          acc.push(site.customScreenshotURL);
        }
        return acc;
      }, [])
    );
  }

  /**
   * shouldFilterSearchTile - is default filtering enabled and does a given hostname match the user's default search engine?
   *
   * @param {string} hostname a top site hostname, such as "amazon" or "foo"
   * @returns {bool}
   */
  shouldFilterSearchTile(hostname) {
    if (
      this.store.getState().Prefs.values[FILTER_DEFAULT_SEARCH_PREF] &&
      (SEARCH_FILTERS.includes(hostname) ||
        hostname === this._currentSearchHostname)
    ) {
      return true;
    }
    return false;
  }

  /**
   * _maybeInsertSearchShortcuts - if the search shortcuts experiment is running,
   *                               insert search shortcuts if needed
   * @param {Array} plainPinnedSites (from the pinnedSitesCache)
   * @returns {Boolean} Did we insert any search shortcuts?
   */
  async _maybeInsertSearchShortcuts(plainPinnedSites) {
    // Only insert shortcuts if the experiment is running
    if (this.store.getState().Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT]) {
      // We don't want to insert shortcuts we've previously inserted
      const prevInsertedShortcuts = this.store
        .getState()
        .Prefs.values[SEARCH_SHORTCUTS_HAVE_PINNED_PREF].split(",")
        .filter(s => s); // Filter out empty strings
      const newInsertedShortcuts = [];

      let shouldPin = this._useRemoteSetting
        ? DEFAULT_TOP_SITES.filter(s => s.searchTopSite).map(s => s.hostname)
        : this.store
            .getState()
            .Prefs.values[SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF].split(",");
      shouldPin = shouldPin
        .map(getSearchProvider)
        .filter(s => s && s.shortURL !== this._currentSearchHostname);

      // If we've previously inserted all search shortcuts return early
      if (
        shouldPin.every(shortcut =>
          prevInsertedShortcuts.includes(shortcut.shortURL)
        )
      ) {
        return false;
      }

      const numberOfSlots =
        this.store.getState().Prefs.values[ROWS_PREF] *
        TOP_SITES_MAX_SITES_PER_ROW;

      // The plainPinnedSites array is populated with pinned sites at their
      // respective indices, and null everywhere else, but is not always the
      // right length
      const emptySlots = Math.max(numberOfSlots - plainPinnedSites.length, 0);
      const pinnedSites = [...plainPinnedSites].concat(
        Array(emptySlots).fill(null)
      );

      const tryToInsertSearchShortcut = async shortcut => {
        const nextAvailable = pinnedSites.indexOf(null);
        // Only add a search shortcut if the site isn't already pinned, we
        // haven't previously inserted it, there's space to pin it, and the
        // search engine is available in Firefox
        if (
          !pinnedSites.find(s => s && shortURL(s) === shortcut.shortURL) &&
          !prevInsertedShortcuts.includes(shortcut.shortURL) &&
          nextAvailable > -1 &&
          (await checkHasSearchEngine(shortcut.keyword))
        ) {
          const site = await this.topSiteToSearchTopSite({ url: shortcut.url });
          this._pinSiteAt(site, nextAvailable);
          pinnedSites[nextAvailable] = site;
          newInsertedShortcuts.push(shortcut.shortURL);
        }
      };

      for (let shortcut of shouldPin) {
        await tryToInsertSearchShortcut(shortcut);
      }

      if (newInsertedShortcuts.length) {
        this.store.dispatch(
          ac.SetPref(
            SEARCH_SHORTCUTS_HAVE_PINNED_PREF,
            prevInsertedShortcuts.concat(newInsertedShortcuts).join(",")
          )
        );
        return true;
      }
    }

    return false;
  }

  /**
   * Fetch topsites spocs from the DiscoveryStream feed.
   *
   * @returns {Array} An array of sponsored tile objects.
   */
  fetchDiscoveryStreamSpocs() {
    let sponsored = [];
    const { DiscoveryStream } = this.store.getState();
    if (DiscoveryStream) {
      const discoveryStreamSpocs =
        DiscoveryStream.spocs.data["sponsored-topsites"]?.items || [];
      // Find the first component of a type and remove it from layout
      const findSponsoredTopsitesPositions = name => {
        for (const row of DiscoveryStream.layout) {
          for (const component of row.components) {
            if (component.placement?.name === name) {
              return component.spocs.positions;
            }
          }
        }
        return null;
      };

      // Get positions from layout for now. This could be improved if we store position data in state.
      const discoveryStreamSpocPositions =
        findSponsoredTopsitesPositions("sponsored-topsites");

      if (discoveryStreamSpocPositions?.length) {
        function reformatImageURL(url, width, height) {
          // Change the image URL to request a size tailored for the parent container width
          // Also: force JPEG, quality 60, no upscaling, no EXIF data
          // Uses Thumbor: https://thumbor.readthedocs.io/en/latest/usage.html
          // For now we wrap this in single quotes because this is being used in a url() css rule, and otherwise would cause a parsing error.
          return `'https://img-getpocket.cdn.mozilla.net/${width}x${height}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(
            url
          )}'`;
        }

        // We need to loop through potential spocs and set their positions.
        // If we run out of spocs or positions, we stop.
        // First, we need to know which array is shortest. This is our exit condition.
        const minLength = Math.min(
          discoveryStreamSpocPositions.length,
          discoveryStreamSpocs.length
        );
        // Loop until we run out of spocs or positions.
        for (let i = 0; i < minLength; i++) {
          const positionIndex = discoveryStreamSpocPositions[i].index;
          const spoc = discoveryStreamSpocs[i];
          const link = {
            favicon: reformatImageURL(spoc.raw_image_src, 96, 96),
            faviconSize: 96,
            type: "SPOC",
            label: spoc.title || spoc.sponsor,
            title: spoc.title || spoc.sponsor,
            url: spoc.url,
            flightId: spoc.flight_id,
            id: spoc.id,
            guid: spoc.id,
            shim: spoc.shim,
            // For now we are assuming position based on intended position.
            // Actual position can shift based on other content.
            // We send the intended position in the ping.
            pos: positionIndex,
            // Set this so that SPOC topsites won't be shown in the URL bar.
            // See Bug 1822027. Note that `sponsored_position` is 1-based.
            sponsored_position: positionIndex + 1,
            // This is used for topsites deduping.
            hostname: shortURL({ url: spoc.url }),
            partner: SPONSORED_TILE_PARTNER_MOZ_SALES,
          };
          sponsored.push(link);
        }
      }
    }
    return sponsored;
  }

  // eslint-disable-next-line max-statements
  async getLinksWithDefaults(isStartup = false) {
    const prefValues = this.store.getState().Prefs.values;
    const numItems = prefValues[ROWS_PREF] * TOP_SITES_MAX_SITES_PER_ROW;
    const searchShortcutsExperiment = prefValues[SEARCH_SHORTCUTS_EXPERIMENT];
    // We must wait for search services to initialize in order to access default
    // search engine properties without triggering a synchronous initialization
    try {
      await Services.search.init();
    } catch {
      // We continue anyway because we want the user to see their sponsored,
      // saved, or visited shortcut tiles even if search engines are not
      // available.
    }

    // Get all frecent sites from history.
    let frecent = [];
    const cache = await this.frecentCache.request({
      // We need to overquery due to the top 5 alexa search + default search possibly being removed
      numItems: numItems + SEARCH_FILTERS.length + 1,
      topsiteFrecency: FRECENCY_THRESHOLD,
    });
    for (let link of cache) {
      const hostname = shortURL(link);
      if (!this.shouldFilterSearchTile(hostname)) {
        frecent.push({
          ...(searchShortcutsExperiment
            ? await this.topSiteToSearchTopSite(link)
            : link),
          hostname,
        });
      }
    }

    // Get defaults.
    let contileSponsored = [];
    let notBlockedDefaultSites = [];
    for (let link of DEFAULT_TOP_SITES) {
      // For sponsored Yandex links, default filtering is reversed: we only
      // show them if Yandex is the default search engine.
      if (link.sponsored_position && link.hostname === "yandex") {
        if (link.hostname !== this._currentSearchHostname) {
          continue;
        }
      } else if (this.shouldFilterSearchTile(link.hostname)) {
        continue;
      }
      // Drop blocked default sites.
      if (
        lazy.NewTabUtils.blockedLinks.isBlocked({
          url: link.url,
        })
      ) {
        continue;
      }
      // If we've previously blocked a search shortcut, remove the default top site
      // that matches the hostname
      const searchProvider = getSearchProvider(shortURL(link));
      if (
        searchProvider &&
        lazy.NewTabUtils.blockedLinks.isBlocked({ url: searchProvider.url })
      ) {
        continue;
      }
      if (link.sponsored_position) {
        if (!prefValues[SHOW_SPONSORED_PREF]) {
          continue;
        }
        contileSponsored[link.sponsored_position - 1] = link;

        // Unpin search shortcut if present for the sponsored link to be shown
        // instead.
        this._unpinSearchShortcut(link.hostname);
      } else {
        notBlockedDefaultSites.push(
          searchShortcutsExperiment
            ? await this.topSiteToSearchTopSite(link)
            : link
        );
      }
    }

    const discoverySponsored = this.fetchDiscoveryStreamSpocs();

    const sponsored = this._mergeSponsoredLinks({
      [SPONSORED_TILE_PARTNER_AMP]: contileSponsored,
      [SPONSORED_TILE_PARTNER_MOZ_SALES]: discoverySponsored,
    });

    this._maybeCapSponsoredLinks(sponsored);

    // Get pinned links augmented with desired properties
    let plainPinned = await this.pinnedCache.request();

    // Insert search shortcuts if we need to.
    // _maybeInsertSearchShortcuts returns true if any search shortcuts are
    // inserted, meaning we need to expire and refresh the pinnedCache
    if (await this._maybeInsertSearchShortcuts(plainPinned)) {
      this.pinnedCache.expire();
      plainPinned = await this.pinnedCache.request();
    }

    const pinned = await Promise.all(
      plainPinned.map(async link => {
        if (!link) {
          return link;
        }

        // Drop pinned search shortcuts when their engine has been removed / hidden.
        if (link.searchTopSite) {
          const searchProvider = getSearchProvider(shortURL(link));
          if (
            !searchProvider ||
            !(await checkHasSearchEngine(searchProvider.keyword))
          ) {
            return null;
          }
        }

        // Copy all properties from a frecent link and add more
        const finder = other => other.url === link.url;

        // Remove frecent link's screenshot if pinned link has a custom one
        const frecentSite = frecent.find(finder);
        if (frecentSite && link.customScreenshotURL) {
          delete frecentSite.screenshot;
        }
        // If the link is a frecent site, do not copy over 'isDefault', else check
        // if the site is a default site
        const copy = Object.assign(
          {},
          frecentSite || { isDefault: !!notBlockedDefaultSites.find(finder) },
          link,
          { hostname: shortURL(link) },
          { searchTopSite: !!link.searchTopSite }
        );

        // Add in favicons if we don't already have it
        if (!copy.favicon) {
          try {
            lazy.NewTabUtils.activityStreamProvider._faviconBytesToDataURI(
              await lazy.NewTabUtils.activityStreamProvider._addFavicons([copy])
            );

            for (const prop of PINNED_FAVICON_PROPS_TO_MIGRATE) {
              copy.__sharedCache.updateLink(prop, copy[prop]);
            }
          } catch (e) {
            // Some issue with favicon, so just continue without one
          }
        }

        return copy;
      })
    );

    // Remove any duplicates from frecent and default sites
    const [, dedupedSponsored, dedupedFrecent, dedupedDefaults] =
      this.dedupe.group(pinned, sponsored, frecent, notBlockedDefaultSites);
    const dedupedUnpinned = [...dedupedFrecent, ...dedupedDefaults];

    // Remove adult sites if we need to
    const checkedAdult = lazy.FilterAdult.filter(dedupedUnpinned);

    // Insert the original pinned sites into the deduped frecent and defaults.
    let withPinned = insertPinned(checkedAdult, pinned);
    // Insert sponsored sites at their desired position.
    dedupedSponsored.forEach(link => {
      if (!link) {
        return;
      }
      let index = link.sponsored_position - 1;
      if (index >= withPinned.length) {
        withPinned[index] = link;
      } else if (withPinned[index]?.sponsored_position) {
        // We currently want DiscoveryStream spocs to replace existing spocs.
        withPinned[index] = link;
      } else {
        withPinned.splice(index, 0, link);
      }
    });

    // Remove excess items after we inserted sponsored ones.
    withPinned = withPinned.slice(0, numItems);

    // Now, get a tippy top icon, a rich icon, or screenshot for every item
    for (const link of withPinned) {
      if (link) {
        // If there is a custom screenshot this is the only image we display
        if (link.customScreenshotURL) {
          this._fetchScreenshot(link, link.customScreenshotURL, isStartup);
        } else if (link.searchTopSite && !link.isDefault) {
          await this._attachTippyTopIconForSearchShortcut(link, link.label);
        } else {
          this._fetchIcon(link, isStartup);
        }

        // Remove internal properties that might be updated after dispatch
        delete link.__sharedCache;

        // Indicate that these links should get a frecency bonus when clicked
        link.typedBonus = true;
      }
    }

    this._linksWithDefaults = withPinned;

    return withPinned;
  }

  /**
   * Cap sponsored links if they're more than the specified maximum.
   *
   * @param {Array} links An array of sponsored links. Capping will be performed in-place.
   */
  _maybeCapSponsoredLinks(links) {
    // Set maximum sponsored top sites
    const maxSponsored =
      lazy.NimbusFeatures.pocketNewtab.getVariable(
        NIMBUS_VARIABLE_MAX_SPONSORED
      ) ?? MAX_NUM_SPONSORED;
    if (links.length > maxSponsored) {
      links.length = maxSponsored;
    }
  }

  /**
   * Merge sponsored links from all the partners using SOV if present.
   * For each tile position, the user is assigned to one partner via stable sampling.
   * If the chosen partner doesn't have a tile to serve, another tile from a different
   * partner is used as the replacement.
   *
   * @param {Object} sponsoredLinks An object with sponsored links from all the partners.
   * @returns {Array} An array of merged sponsored links.
   */
  _mergeSponsoredLinks(sponsoredLinks) {
    const { positions: allocatedPositions, ready: sovReady } =
      this.store.getState().TopSites.sov || {};
    if (
      !this._contile.sov ||
      !sovReady ||
      !lazy.NimbusFeatures.pocketNewtab.getVariable(
        NIMBUS_VARIABLE_CONTILE_SOV_ENABLED
      )
    ) {
      return Object.values(sponsoredLinks).flat();
    }

    // AMP links might have empty slots, remove them as SOV doesn't need those.
    sponsoredLinks[SPONSORED_TILE_PARTNER_AMP] =
      sponsoredLinks[SPONSORED_TILE_PARTNER_AMP].filter(Boolean);

    let sponsored = [];
    let chosenPartners = [];

    for (const allocation of allocatedPositions) {
      let link = null;
      const { assignedPartner } = allocation;
      if (assignedPartner) {
        // Unknown partners are allowed so that new parters can be added to Shepherd
        // sooner without waiting for client changes.
        link = sponsoredLinks[assignedPartner]?.shift();
      }

      if (!link) {
        // If the chosen partner doesn't have a tile for this postion, choose any
        // one from another group. For simplicity, we do _not_ do resampling here
        // against the remaining partners.
        for (const partner of SPONSORED_TILE_PARTNERS) {
          if (
            partner === assignedPartner ||
            sponsoredLinks[partner].length === 0
          ) {
            continue;
          }
          link = sponsoredLinks[partner].shift();
          break;
        }

        if (!link) {
          // No more links to be added across all the partners, just return.
          if (chosenPartners.length) {
            Glean.newtab.sovAllocation.set(
              chosenPartners.map(entry => JSON.stringify(entry))
            );
          }
          return sponsored;
        }
      }

      // Update the position fields. Note that postion is also 1-based in SOV.
      link.sponsored_position = allocation.position;
      if (link.pos !== undefined) {
        // Pocket `pos` is 0-based.
        link.pos = allocation.position - 1;
      }
      sponsored.push(link);

      chosenPartners.push({
        pos: allocation.position,
        assigned: assignedPartner, // The assigned partner based on SOV
        chosen: link.partner,
      });
    }
    // Record chosen partners to glean
    if (chosenPartners.length) {
      Glean.newtab.sovAllocation.set(
        chosenPartners.map(entry => JSON.stringify(entry))
      );
    }

    // add the remaining contile sponsoredLinks when nimbus variable present
    if (
      lazy.NimbusFeatures.pocketNewtab.getVariable(
        NIMBUS_VARIABLE_CONTILE_MAX_NUM_SPONSORED
      )
    ) {
      return sponsored.concat(sponsoredLinks[SPONSORED_TILE_PARTNER_AMP]);
    }

    return sponsored;
  }

  /**
   * Attach TippyTop icon to the given search shortcut
   *
   * Note that it queries the search form URL from search service For Yandex,
   * and uses it to choose the best icon for its shortcut variants.
   *
   * @param {Object} link A link object with a `url` property
   * @param {string} keyword Search keyword
   */
  async _attachTippyTopIconForSearchShortcut(link, keyword) {
    if (
      ["@\u044F\u043D\u0434\u0435\u043A\u0441", "@yandex"].includes(keyword)
    ) {
      let site = { url: link.url };
      site.url = (await getSearchFormURL(keyword)) || site.url;
      this._tippyTopProvider.processSite(site);
      link.tippyTopIcon = site.tippyTopIcon;
      link.smallFavicon = site.smallFavicon;
      link.backgroundColor = site.backgroundColor;
    } else {
      this._tippyTopProvider.processSite(link);
    }
  }

  /**
   * Refresh the top sites data for content.
   * @param {bool} options.broadcast Should the update be broadcasted.
   * @param {bool} options.isStartup Being called while TopSitesFeed is initting.
   */
  async refresh(options = {}) {
    if (!this._startedUp && !options.isStartup) {
      // Initial refresh still pending.
      return;
    }
    this._startedUp = true;

    if (!this._tippyTopProvider.initialized) {
      await this._tippyTopProvider.init();
    }

    const links = await this.getLinksWithDefaults({
      isStartup: options.isStartup,
    });
    const newAction = { type: at.TOP_SITES_UPDATED, data: { links } };
    let storedPrefs;
    try {
      storedPrefs = (await this._storage.get(SECTION_ID)) || {};
    } catch (e) {
      storedPrefs = {};
      console.error("Problem getting stored prefs for TopSites");
    }
    newAction.data.pref = getDefaultOptions(storedPrefs);

    if (options.isStartup) {
      newAction.meta = {
        isStartup: true,
      };
    }

    if (options.broadcast) {
      // Broadcast an update to all open content pages
      this.store.dispatch(ac.BroadcastToContent(newAction));
    } else {
      // Don't broadcast only update the state and update the preloaded tab.
      this.store.dispatch(ac.AlsoToPreloaded(newAction));
    }
  }

  // Allocate ad positions to partners based on SOV via stable randomization.
  async allocatePositions() {
    // If the fetch to get sov fails for whatever reason, we can just return here.
    // Code that uses this falls back to flattening allocations instead if this has failed.
    if (!this._contile.sov) {
      return;
    }
    // This sample input should ensure we return the same result for this allocation,
    // even if called from other parts of the code.
    const sampleInput = `${lazy.contextId}-${this._contile.sov.name}`;
    const allocatedPositions = [];
    for (const allocation of this._contile.sov.allocations) {
      const allocatedPosition = {
        position: allocation.position,
      };
      allocatedPositions.push(allocatedPosition);
      const ratios = allocation.allocation.map(alloc => alloc.percentage);
      if (ratios.length) {
        const index = await lazy.Sampling.ratioSample(sampleInput, ratios);
        allocatedPosition.assignedPartner =
          allocation.allocation[index].partner;
      }
    }

    this.store.dispatch(
      ac.OnlyToMain({
        type: at.SOV_UPDATED,
        data: {
          ready: !!allocatedPositions.length,
          positions: allocatedPositions,
        },
      })
    );
  }

  async updateCustomSearchShortcuts(isStartup = false) {
    if (!this.store.getState().Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT]) {
      return;
    }

    if (!this._tippyTopProvider.initialized) {
      await this._tippyTopProvider.init();
    }

    // Populate the state with available search shortcuts
    let searchShortcuts = [];
    for (const engine of await Services.search.getAppProvidedEngines()) {
      const shortcut = CUSTOM_SEARCH_SHORTCUTS.find(s =>
        engine.aliases.includes(s.keyword)
      );
      if (shortcut) {
        let clone = { ...shortcut };
        await this._attachTippyTopIconForSearchShortcut(clone, clone.keyword);
        searchShortcuts.push(clone);
      }
    }

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.UPDATE_SEARCH_SHORTCUTS,
        data: { searchShortcuts },
        meta: {
          isStartup,
        },
      })
    );
  }

  async topSiteToSearchTopSite(site) {
    const searchProvider = getSearchProvider(shortURL(site));
    if (
      !searchProvider ||
      !(await checkHasSearchEngine(searchProvider.keyword))
    ) {
      return site;
    }
    return {
      ...site,
      searchTopSite: true,
      label: searchProvider.keyword,
    };
  }

  /**
   * Get an image for the link preferring tippy top, rich favicon, screenshots.
   */
  async _fetchIcon(link, isStartup = false) {
    // Nothing to do if we already have a rich icon from the page
    if (link.favicon && link.faviconSize >= MIN_FAVICON_SIZE) {
      return;
    }

    // Nothing more to do if we can use a default tippy top icon
    this._tippyTopProvider.processSite(link);
    if (link.tippyTopIcon) {
      return;
    }

    // Make a request for a better icon
    this._requestRichIcon(link.url);

    // Also request a screenshot if we don't have one yet
    await this._fetchScreenshot(link, link.url, isStartup);
  }

  /**
   * Fetch, cache and broadcast a screenshot for a specific topsite.
   * @param link cached topsite object
   * @param url where to fetch the image from
   * @param isStartup Whether the screenshot is fetched while initting TopSitesFeed.
   */
  async _fetchScreenshot(link, url, isStartup = false) {
    // We shouldn't bother caching screenshots if they won't be shown.
    if (
      link.screenshot ||
      !this.store.getState().Prefs.values[SHOWN_ON_NEWTAB_PREF]
    ) {
      return;
    }
    await lazy.Screenshots.maybeCacheScreenshot(
      link,
      url,
      "screenshot",
      screenshot =>
        this.store.dispatch(
          ac.BroadcastToContent({
            data: { screenshot, url: link.url },
            type: at.SCREENSHOT_UPDATED,
            meta: {
              isStartup,
            },
          })
        )
    );
  }

  /**
   * Dispatch screenshot preview to target or notify if request failed.
   * @param customScreenshotURL {string} The URL used to capture the screenshot
   * @param target {string} Id of content process where to dispatch the result
   */
  async getScreenshotPreview(url, target) {
    const preview = (await lazy.Screenshots.getScreenshotForURL(url)) || "";
    this.store.dispatch(
      ac.OnlyToOneContent(
        {
          data: { url, preview },
          type: at.PREVIEW_RESPONSE,
        },
        target
      )
    );
  }

  _requestRichIcon(url) {
    this.store.dispatch({
      type: at.RICH_ICON_MISSING,
      data: { url },
    });
  }

  updateSectionPrefs(collapsed) {
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.TOP_SITES_PREFS_UPDATED,
        data: { pref: collapsed },
      })
    );
  }

  /**
   * Inform others that top sites data has been updated due to pinned changes.
   */
  _broadcastPinnedSitesUpdated() {
    // Pinned data changed, so make sure we get latest
    this.pinnedCache.expire();

    // Refresh to update pinned sites with screenshots, trigger deduping, etc.
    this.refresh({ broadcast: true });
  }

  /**
   * Pin a site at a specific position saving only the desired keys.
   * @param customScreenshotURL {string} User set URL of preview image for site
   * @param label {string} User set string of custom site name
   */
  async _pinSiteAt({ customScreenshotURL, label, url, searchTopSite }, index) {
    const toPin = { url };
    if (label) {
      toPin.label = label;
    }
    if (customScreenshotURL) {
      toPin.customScreenshotURL = customScreenshotURL;
    }
    if (searchTopSite) {
      toPin.searchTopSite = searchTopSite;
    }
    lazy.NewTabUtils.pinnedLinks.pin(toPin, index);

    await this._clearLinkCustomScreenshot({ customScreenshotURL, url });
  }

  async _clearLinkCustomScreenshot(site) {
    // If screenshot url changed or was removed we need to update the cached link obj
    if (site.customScreenshotURL !== undefined) {
      const pinned = await this.pinnedCache.request();
      const link = pinned.find(pin => pin && pin.url === site.url);
      if (link && link.customScreenshotURL !== site.customScreenshotURL) {
        link.__sharedCache.updateLink("screenshot", undefined);
      }
    }
  }

  /**
   * Handle a pin action of a site to a position.
   */
  async pin(action) {
    let { site, index } = action.data;
    index = this._adjustPinIndexForSponsoredLinks(site, index);
    // If valid index provided, pin at that position
    if (index >= 0) {
      await this._pinSiteAt(site, index);
      this._broadcastPinnedSitesUpdated();
    } else {
      // Bug 1458658. If the top site is being pinned from an 'Add a Top Site' option,
      // then we want to make sure to unblock that link if it has previously been
      // blocked. We know if the site has been added because the index will be -1.
      if (index === -1) {
        lazy.NewTabUtils.blockedLinks.unblock({ url: site.url });
        this.frecentCache.expire();
      }
      this.insert(action);
    }
  }

  /**
   * Handle an unpin action of a site.
   */
  unpin(action) {
    const { site } = action.data;
    lazy.NewTabUtils.pinnedLinks.unpin(site);
    this._broadcastPinnedSitesUpdated();
  }

  unpinAllSearchShortcuts() {
    Services.prefs.clearUserPref(
      `browser.newtabpage.activity-stream.${SEARCH_SHORTCUTS_HAVE_PINNED_PREF}`
    );
    for (let pinnedLink of lazy.NewTabUtils.pinnedLinks.links) {
      if (pinnedLink && pinnedLink.searchTopSite) {
        lazy.NewTabUtils.pinnedLinks.unpin(pinnedLink);
      }
    }
    this.pinnedCache.expire();
  }

  _unpinSearchShortcut(vendor) {
    for (let pinnedLink of lazy.NewTabUtils.pinnedLinks.links) {
      if (
        pinnedLink &&
        pinnedLink.searchTopSite &&
        shortURL(pinnedLink) === vendor
      ) {
        lazy.NewTabUtils.pinnedLinks.unpin(pinnedLink);
        this.pinnedCache.expire();

        const prevInsertedShortcuts = this.store
          .getState()
          .Prefs.values[SEARCH_SHORTCUTS_HAVE_PINNED_PREF].split(",");
        this.store.dispatch(
          ac.SetPref(
            SEARCH_SHORTCUTS_HAVE_PINNED_PREF,
            prevInsertedShortcuts.filter(s => s !== vendor).join(",")
          )
        );
        break;
      }
    }
  }

  /**
   * Reduces the given pinning index by the number of preceding sponsored
   * sites, to accomodate for sponsored sites pushing pinned ones to the side,
   * effectively increasing their index again.
   */
  _adjustPinIndexForSponsoredLinks(site, index) {
    if (!this._linksWithDefaults) {
      return index;
    }
    // Adjust insertion index for sponsored sites since their position is
    // fixed.
    let adjustedIndex = index;
    for (let i = 0; i < index; i++) {
      const link = this._linksWithDefaults[i];
      if (
        link &&
        link.sponsored_position &&
        this._linksWithDefaults[i]?.url !== site.url
      ) {
        adjustedIndex--;
      }
    }
    return adjustedIndex;
  }

  /**
   * Insert a site to pin at a position shifting over any other pinned sites.
   */
  _insertPin(site, originalIndex, draggedFromIndex) {
    let index = this._adjustPinIndexForSponsoredLinks(site, originalIndex);

    // Don't insert any pins past the end of the visible top sites. Otherwise,
    // we can end up with a bunch of pinned sites that can never be unpinned again
    // from the UI.
    const topSitesCount =
      this.store.getState().Prefs.values[ROWS_PREF] *
      TOP_SITES_MAX_SITES_PER_ROW;
    if (index >= topSitesCount) {
      return;
    }

    let pinned = lazy.NewTabUtils.pinnedLinks.links;
    if (!pinned[index]) {
      this._pinSiteAt(site, index);
    } else {
      pinned[draggedFromIndex] = null;
      // Find the hole to shift the pinned site(s) towards. We shift towards the
      // hole left by the site being dragged.
      let holeIndex = index;
      const indexStep = index > draggedFromIndex ? -1 : 1;
      while (pinned[holeIndex]) {
        holeIndex += indexStep;
      }
      if (holeIndex >= topSitesCount || holeIndex < 0) {
        // There are no holes, so we will effectively unpin the last slot and shifting
        // towards it. This only happens when adding a new top site to an already
        // fully pinned grid.
        holeIndex = topSitesCount - 1;
      }

      // Shift towards the hole.
      const shiftingStep = holeIndex > index ? -1 : 1;
      while (holeIndex !== index) {
        const nextIndex = holeIndex + shiftingStep;
        this._pinSiteAt(pinned[nextIndex], holeIndex);
        holeIndex = nextIndex;
      }
      this._pinSiteAt(site, index);
    }
  }

  /**
   * Handle an insert (drop/add) action of a site.
   */
  async insert(action) {
    let { index } = action.data;
    // Treat invalid pin index values (e.g., -1, undefined) as insert in the first position
    if (!(index > 0)) {
      index = 0;
    }

    // Inserting a top site pins it in the specified slot, pushing over any link already
    // pinned in the slot (unless it's the last slot, then it replaces).
    this._insertPin(
      action.data.site,
      index,
      action.data.draggedFromIndex !== undefined
        ? action.data.draggedFromIndex
        : this.store.getState().Prefs.values[ROWS_PREF] *
            TOP_SITES_MAX_SITES_PER_ROW
    );

    await this._clearLinkCustomScreenshot(action.data.site);
    this._broadcastPinnedSitesUpdated();
  }

  updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts }) {
    // Unpin the deletedShortcuts.
    deletedShortcuts.forEach(({ url }) => {
      lazy.NewTabUtils.pinnedLinks.unpin({ url });
    });

    // Pin the addedShortcuts.
    const numberOfSlots =
      this.store.getState().Prefs.values[ROWS_PREF] *
      TOP_SITES_MAX_SITES_PER_ROW;
    addedShortcuts.forEach(shortcut => {
      // Find first hole in pinnedLinks.
      let index = lazy.NewTabUtils.pinnedLinks.links.findIndex(link => !link);
      if (
        index < 0 &&
        lazy.NewTabUtils.pinnedLinks.links.length + 1 < numberOfSlots
      ) {
        // pinnedLinks can have less slots than the total available.
        index = lazy.NewTabUtils.pinnedLinks.links.length;
      }
      if (index >= 0) {
        lazy.NewTabUtils.pinnedLinks.pin(shortcut, index);
      } else {
        // No slots available, we need to do an insert in first slot and push over other pinned links.
        this._insertPin(shortcut, 0, numberOfSlots);
      }
    });

    this._broadcastPinnedSitesUpdated();
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        this.updateCustomSearchShortcuts(true /* isStartup */);
        break;
      case at.SYSTEM_TICK:
        this.refresh({ broadcast: false });
        this._contile.periodicUpdate();
        break;
      // All these actions mean we need new top sites
      case at.PLACES_HISTORY_CLEARED:
      case at.PLACES_LINKS_DELETED:
        this.frecentCache.expire();
        this.refresh({ broadcast: true });
        break;
      case at.PLACES_LINKS_CHANGED:
        this.frecentCache.expire();
        this.refresh({ broadcast: false });
        break;
      case at.PLACES_LINK_BLOCKED:
        this.frecentCache.expire();
        this.pinnedCache.expire();
        this.refresh({ broadcast: true });
        break;
      case at.PREF_CHANGED:
        switch (action.data.name) {
          case DEFAULT_SITES_PREF:
            if (!this._useRemoteSetting) {
              this.refreshDefaults(action.data.value);
            }
            break;
          case ROWS_PREF:
          case FILTER_DEFAULT_SEARCH_PREF:
          case SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF:
            this.refresh({ broadcast: true });
            break;
          case SHOW_SPONSORED_PREF:
            if (
              lazy.NimbusFeatures.newtab.getVariable(
                NIMBUS_VARIABLE_CONTILE_ENABLED
              )
            ) {
              this._contile.refresh();
            } else {
              this.refresh({ broadcast: true });
            }
            if (!action.data.value) {
              this._contile._resetContileCachePrefs();
            }

            break;
          case SEARCH_SHORTCUTS_EXPERIMENT:
            if (action.data.value) {
              this.updateCustomSearchShortcuts();
            } else {
              this.unpinAllSearchShortcuts();
            }
            this.refresh({ broadcast: true });
        }
        break;
      case at.UPDATE_SECTION_PREFS:
        if (action.data.id === SECTION_ID) {
          this.updateSectionPrefs(action.data.value);
        }
        break;
      case at.PREFS_INITIAL_VALUES:
        if (!this._useRemoteSetting) {
          this.refreshDefaults(action.data[DEFAULT_SITES_PREF]);
        }
        break;
      case at.TOP_SITES_PIN:
        this.pin(action);
        break;
      case at.TOP_SITES_UNPIN:
        this.unpin(action);
        break;
      case at.TOP_SITES_INSERT:
        this.insert(action);
        break;
      case at.PREVIEW_REQUEST:
        this.getScreenshotPreview(action.data.url, action.meta.fromTarget);
        break;
      case at.UPDATE_PINNED_SEARCH_SHORTCUTS:
        this.updatePinnedSearchShortcuts(action.data);
        break;
      case at.DISCOVERY_STREAM_SPOCS_UPDATE:
        // Refresh to update sponsored topsites.
        this.refresh({ broadcast: true, isStartup: action.meta.isStartup });
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
}

const EXPORTED_SYMBOLS = [
  "TopSitesFeed",
  "DEFAULT_TOP_SITES",
  "ContileIntegration",
];
