/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "resource://activity-stream/common/Actions.mjs";
import { TippyTopProvider } from "resource://activity-stream/lib/TippyTopProvider.sys.mjs";
import {
  insertPinned,
  TOP_SITES_MAX_SITES_PER_ROW,
} from "resource://activity-stream/common/Reducers.sys.mjs";
import { Dedupe } from "resource://activity-stream/common/Dedupe.sys.mjs";
import { shortURL } from "resource://activity-stream/lib/ShortURL.sys.mjs";

import {
  CUSTOM_SEARCH_SHORTCUTS,
  SEARCH_SHORTCUTS_EXPERIMENT,
  SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF,
  SEARCH_SHORTCUTS_HAVE_PINNED_PREF,
  checkHasSearchEngine,
  getSearchProvider,
  getSearchFormURL,
} from "resource://activity-stream/lib/SearchShortcuts.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FilterAdult: "resource://activity-stream/lib/FilterAdult.sys.mjs",
  LinksCache: "resource://activity-stream/lib/LinksCache.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  PageThumbs: "resource://gre/modules/PageThumbs.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  Screenshots: "resource://activity-stream/lib/Screenshots.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("TopSites");
});

const DEFAULT_SITES_PREF = "default.sites";
const SHOWN_ON_NEWTAB_PREF = "feeds.topsites";
export const DEFAULT_TOP_SITES = [];
const FRECENCY_THRESHOLD = 100 + 1; // 1 visit (skip first-run/one-time pages)
const MIN_FAVICON_SIZE = 96;
const CACHED_LINK_PROPS_TO_MIGRATE = ["screenshot", "customScreenshot"];
const PINNED_FAVICON_PROPS_TO_MIGRATE = [
  "favicon",
  "faviconRef",
  "faviconSize",
];
const ROWS_PREF = "topSitesRows";

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

function getShortURLForCurrentSearch() {
  const url = shortURL({ url: Services.search.defaultEngine.searchForm });
  return url;
}

class _TopSites {
  constructor() {
    this._tippyTopProvider = new TippyTopProvider();
    ChromeUtils.defineLazyGetter(
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
  }

  init() {
    lazy.log.debug("Initializing TopSites.");
    // If the feed was previously disabled PREFS_INITIAL_VALUES was never received
    this._readDefaults({ isStartup: true });
    Services.obs.addObserver(this, "browser-search-engine-modified");
    Services.obs.addObserver(this, "browser-region-updated");
    Services.prefs.addObserver(REMOTE_SETTING_DEFAULTS_PREF, this);
    Services.prefs.addObserver(DEFAULT_SITES_OVERRIDE_PREF, this);
    Services.prefs.addObserver(DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH, this);
  }

  uninit() {
    lazy.log.debug("Un-initializing TopSites.");
    lazy.PageThumbs.removeExpirationFilter(this);
    Services.obs.removeObserver(this, "browser-search-engine-modified");
    Services.obs.removeObserver(this, "browser-region-updated");
    Services.prefs.removeObserver(REMOTE_SETTING_DEFAULTS_PREF, this);
    Services.prefs.removeObserver(DEFAULT_SITES_OVERRIDE_PREF, this);
    Services.prefs.removeObserver(DEFAULT_SITES_EXPERIMENTS_PREF_BRANCH, this);
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

    // Read defaults from remote settings.
    this._useRemoteSetting = true;
    let remoteSettingData = await this._getRemoteConfig();

    for (let siteData of remoteSettingData) {
      let hostname = shortURL(siteData);
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
   *
   * @param {Array} plainPinnedSites (from the pinnedSitesCache)
   * @returns {boolean} Did we insert any search shortcuts?
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
    let cache;
    try {
      // Request can throw if executing the linkGetter inside LinksCache returns
      // a null object.
      cache = await this.frecentCache.request({
        // We need to overquery due to the top 5 alexa search + default search possibly being removed
        numItems: numItems + SEARCH_FILTERS.length + 1,
        topsiteFrecency: FRECENCY_THRESHOLD,
      });
    } catch (ex) {
      cache = [];
    }

    for (let link of cache) {
      // The cache can contain null values.
      if (!link) {
        continue;
      }
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
    let notBlockedDefaultSites = [];
    for (let link of DEFAULT_TOP_SITES) {
      if (this.shouldFilterSearchTile(link.hostname)) {
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
      notBlockedDefaultSites.push(
        searchShortcutsExperiment
          ? await this.topSiteToSearchTopSite(link)
          : link
      );
    }

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
    const [, dedupedFrecent, dedupedDefaults] = this.dedupe.group(
      pinned,
      frecent,
      notBlockedDefaultSites
    );
    const dedupedUnpinned = [...dedupedFrecent, ...dedupedDefaults];

    // Remove adult sites if we need to
    const checkedAdult = lazy.FilterAdult.filter(dedupedUnpinned);

    // Insert the original pinned sites into the deduped frecent and defaults.
    let withPinned = insertPinned(checkedAdult, pinned);
    // Remove excess items.
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
   * Attach TippyTop icon to the given search shortcut
   *
   * Note that it queries the search form URL from search service For Yandex,
   * and uses it to choose the best icon for its shortcut variants.
   *
   * @param {object} link A link object with a `url` property
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
   *
   * @param {object} options
   * @param {bool} options.broadcast Should the update be broadcasted.
   * @param {bool} options.isStartup Being called while TopSitesFeed is initting.
   */
  async refresh(options = {}) {
    // Avoiding refreshing if it's already happening.
    if (this._refreshing) {
      return;
    }
    if (!this._startedUp && !options.isStartup) {
      // Initial refresh still pending.
      return;
    }
    this._refreshing = true;
    this._startedUp = true;

    if (!this._tippyTopProvider.initialized) {
      await this._tippyTopProvider.init();
    }

    const links = await this.getLinksWithDefaults({
      isStartup: options.isStartup,
    });
    const newAction = {
      type: at.TOP_SITES_UPDATED,
      data: { links },
    };

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
    this._refreshing = false;
    if (Cu.isInAutomation) {
      Services.obs.notifyObservers(null, "topsites-refreshed");
    }
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
   *
   * @param {object} link cached topsite object
   * @param {string} url where to fetch the image from
   * @param {boolean} isStartup Whether the screenshot is fetched while
   * initting TopSitesFeed.
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
   *
   * @param {string} url The URL used to capture the screenshot
   * @param {string} target Id of content process where to dispatch the result
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
   *
   * @param customScreenshotURL {string} User set URL of preview image for site
   * @param label {string} User set string of custom site name
   */
  // To refactor in Bug 1891997
  /* eslint-enable jsdoc/check-param-names */
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
          case SEARCH_SHORTCUTS_EXPERIMENT:
            if (action.data.value) {
              this.updateCustomSearchShortcuts();
            } else {
              this.unpinAllSearchShortcuts();
            }
            this.refresh({ broadcast: true });
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
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
}

export const TopSites = new _TopSites();
