/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {TippyTopProvider} = ChromeUtils.import("resource://activity-stream/lib/TippyTopProvider.jsm", {});
const {insertPinned, TOP_SITES_MAX_SITES_PER_ROW} = ChromeUtils.import("resource://activity-stream/common/Reducers.jsm", {});
const {Dedupe} = ChromeUtils.import("resource://activity-stream/common/Dedupe.jsm", {});
const {shortURL} = ChromeUtils.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {getDefaultOptions} = ChromeUtils.import("resource://activity-stream/lib/ActivityStreamStorage.jsm", {});

ChromeUtils.defineModuleGetter(this, "filterAdult",
  "resource://activity-stream/lib/FilterAdult.jsm");
ChromeUtils.defineModuleGetter(this, "LinksCache",
  "resource://activity-stream/lib/LinksCache.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm");
ChromeUtils.defineModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");

const DEFAULT_SITES_PREF = "default.sites";
const DEFAULT_TOP_SITES = [];
const FRECENCY_THRESHOLD = 100 + 1; // 1 visit (skip first-run/one-time pages)
const MIN_FAVICON_SIZE = 96;
const CACHED_LINK_PROPS_TO_MIGRATE = ["screenshot", "customScreenshot"];
const PINNED_FAVICON_PROPS_TO_MIGRATE = ["favicon", "faviconRef", "faviconSize"];
const SECTION_ID = "topsites";
const ROWS_PREF = "topSitesRows";

this.TopSitesFeed = class TopSitesFeed {
  constructor() {
    this._tippyTopProvider = new TippyTopProvider();
    this.dedupe = new Dedupe(this._dedupeKey);
    this.frecentCache = new LinksCache(NewTabUtils.activityStreamLinks,
      "getTopSites", CACHED_LINK_PROPS_TO_MIGRATE, (oldOptions, newOptions) =>
        // Refresh if no old options or requesting more items
        !(oldOptions.numItems >= newOptions.numItems));
    this.pinnedCache = new LinksCache(NewTabUtils.pinnedLinks, "links",
      [...CACHED_LINK_PROPS_TO_MIGRATE, ...PINNED_FAVICON_PROPS_TO_MIGRATE]);
    PageThumbs.addExpirationFilter(this);
  }

  init() {
    // If the feed was previously disabled PREFS_INITIAL_VALUES was never received
    this.refreshDefaults(this.store.getState().Prefs.values[DEFAULT_SITES_PREF]);
    this._storage = this.store.dbStorage.getDbTable("sectionPrefs");
    this.refresh({broadcast: true});
  }

  uninit() {
    PageThumbs.removeExpirationFilter(this);
  }

  _dedupeKey(site) {
    return site && site.hostname;
  }

  refreshDefaults(sites) {
    // Clear out the array of any previous defaults
    DEFAULT_TOP_SITES.length = 0;

    // Add default sites if any based on the pref
    if (sites) {
      for (const url of sites.split(",")) {
        const site = {
          isDefault: true,
          url
        };
        site.hostname = shortURL(site);
        DEFAULT_TOP_SITES.push(site);
      }
    }
  }

  filterForThumbnailExpiration(callback) {
    const {rows} = this.store.getState().TopSites;
    callback(rows.reduce((acc, site) => {
      acc.push(site.url);
      if (site.customScreenshotURL) {
        acc.push(site.customScreenshotURL);
      }
      return acc;
    }, []));
  }

  async getLinksWithDefaults() {
    const numItems = this.store.getState().Prefs.values[ROWS_PREF] * TOP_SITES_MAX_SITES_PER_ROW;
    const frecent = (await this.frecentCache.request({
      numItems,
      topsiteFrecency: FRECENCY_THRESHOLD
    })).map(link => Object.assign({}, link, {hostname: shortURL(link)}));

    // Remove any defaults that have been blocked
    const notBlockedDefaultSites = DEFAULT_TOP_SITES.filter(link =>
      !NewTabUtils.blockedLinks.isBlocked({url: link.url}));

    // Get pinned links augmented with desired properties
    const plainPinned = await this.pinnedCache.request();
    const pinned = await Promise.all(plainPinned.map(async link => {
      if (!link) {
        return link;
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
      const copy = Object.assign({}, frecentSite ||
        {isDefault: !!notBlockedDefaultSites.find(finder)}, link, {hostname: shortURL(link)});

      // Add in favicons if we don't already have it
      if (!copy.favicon) {
        try {
          NewTabUtils.activityStreamProvider._faviconBytesToDataURI(await
            NewTabUtils.activityStreamProvider._addFavicons([copy]));

          for (const prop of PINNED_FAVICON_PROPS_TO_MIGRATE) {
            copy.__sharedCache.updateLink(prop, copy[prop]);
          }
        } catch (e) {
          // Some issue with favicon, so just continue without one
        }
      }

      return copy;
    }));

    // Remove any duplicates from frecent and default sites
    const [, dedupedFrecent, dedupedDefaults] = this.dedupe.group(
      pinned, frecent, notBlockedDefaultSites);
    const dedupedUnpinned = [...dedupedFrecent, ...dedupedDefaults];

    // Remove adult sites if we need to
    const checkedAdult = this.store.getState().Prefs.values.filterAdult ?
      filterAdult(dedupedUnpinned) : dedupedUnpinned;

    // Insert the original pinned sites into the deduped frecent and defaults
    const withPinned = insertPinned(checkedAdult, pinned).slice(0, numItems);

    // Now, get a tippy top icon, a rich icon, or screenshot for every item
    for (const link of withPinned) {
      if (link) {
        // If there is a custom screenshot this is the only image we display
        if (link.customScreenshotURL) {
          this._fetchScreenshot(link, link.customScreenshotURL);
        } else {
          this._fetchIcon(link);
        }

        // Remove internal properties that might be updated after dispatch
        delete link.__sharedCache;

        // Indicate that these links should get a frecency bonus when clicked
        link.typedBonus = true;
      }
    }

    return withPinned;
  }

  /**
   * Refresh the top sites data for content.
   * @param {bool} options.broadcast Should the update be broadcasted.
   */
  async refresh(options = {}) {
    if (!this._tippyTopProvider.initialized) {
      await this._tippyTopProvider.init();
    }
    const links = await this.getLinksWithDefaults();
    const newAction = {type: at.TOP_SITES_UPDATED, data: {links}};
    let storedPrefs;
    try {
      storedPrefs = await this._storage.get(SECTION_ID) || {};
    } catch (e) {
      storedPrefs = {};
      Cu.reportError("Problem getting stored prefs for TopSites");
    }
    newAction.data.pref = getDefaultOptions(storedPrefs);

    if (options.broadcast) {
      // Broadcast an update to all open content pages
      this.store.dispatch(ac.BroadcastToContent(newAction));
    } else {
      // Don't broadcast only update the state and update the preloaded tab.
      this.store.dispatch(ac.AlsoToPreloaded(newAction));
    }
  }

  /**
   * Get an image for the link preferring tippy top, rich favicon, screenshots.
   */
  async _fetchIcon(link) {
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
    await this._fetchScreenshot(link, link.url);
  }

  /**
   * Fetch, cache and broadcast a screenshot for a specific topsite.
   * @param link cached topsite object
   * @param url where to fetch the image from
   */
  async _fetchScreenshot(link, url) {
    if (link.screenshot) {
      return;
    }
    await Screenshots.maybeCacheScreenshot(link, url, "screenshot",
      screenshot => this.store.dispatch(ac.BroadcastToContent({
        data: {screenshot, url: link.url},
        type: at.SCREENSHOT_UPDATED
      })));
  }

  /**
   * Dispatch screenshot preview to target or notify if request failed.
   * @param customScreenshotURL {string} The URL used to capture the screenshot
   * @param target {string} Id of content process where to dispatch the result
   */
  async getScreenshotPreview(url, target) {
    const preview = await Screenshots.getScreenshotForURL(url) || "";
    this.store.dispatch(ac.OnlyToOneContent({
      data: {url, preview},
      type: at.PREVIEW_RESPONSE
    }, target));
  }

  _requestRichIcon(url) {
    this.store.dispatch({
      type: at.RICH_ICON_MISSING,
      data: {url}
    });
  }

  updateSectionPrefs(collapsed) {
    this.store.dispatch(ac.BroadcastToContent({type: at.TOP_SITES_PREFS_UPDATED, data: {pref: collapsed}}));
  }

  /**
   * Inform others that top sites data has been updated due to pinned changes.
   */
  _broadcastPinnedSitesUpdated() {
    // Pinned data changed, so make sure we get latest
    this.pinnedCache.expire();

    // Refresh to update pinned sites with screenshots, trigger deduping, etc.
    this.refresh({broadcast: true});
  }

  /**
   * Pin a site at a specific position saving only the desired keys.
   * @param customScreenshotURL {string} User set URL of preview image for site
   * @param label {string} User set string of custom site name
   */
  async _pinSiteAt({customScreenshotURL, label, url}, index) {
    const toPin = {url};
    if (label) {
      toPin.label = label;
    }
    if (customScreenshotURL) {
      toPin.customScreenshotURL = customScreenshotURL;
    }
    NewTabUtils.pinnedLinks.pin(toPin, index);

    await this._clearLinkCustomScreenshot({customScreenshotURL, url});
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
    const {site, index} = action.data;
    // If valid index provided, pin at that position
    if (index >= 0) {
      await this._pinSiteAt(site, index);
      this._broadcastPinnedSitesUpdated();
    } else {
      // Bug 1458658. If the top site is being pinned from an 'Add a Top Site' option,
      // then we want to make sure to unblock that link if it has previously been
      // blocked. We know if the site has been added because the index will be -1.
      if (index === -1) {
        NewTabUtils.blockedLinks.unblock({url: site.url});
        this.frecentCache.expire();
      }
      this.insert(action);
    }
  }

  /**
   * Handle an unpin action of a site.
   */
  unpin(action) {
    const {site} = action.data;
    NewTabUtils.pinnedLinks.unpin(site);
    this._broadcastPinnedSitesUpdated();
  }

  /**
   * Insert a site to pin at a position shifting over any other pinned sites.
   */
  _insertPin(site, index, draggedFromIndex) {
    // Don't insert any pins past the end of the visible top sites. Otherwise,
    // we can end up with a bunch of pinned sites that can never be unpinned again
    // from the UI.
    const topSitesCount = this.store.getState().Prefs.values[ROWS_PREF] * TOP_SITES_MAX_SITES_PER_ROW;
    if (index >= topSitesCount) {
      return;
    }

    let pinned = NewTabUtils.pinnedLinks.links;
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
    let {index} = action.data;
    // Treat invalid pin index values (e.g., -1, undefined) as insert in the first position
    if (!(index > 0)) {
      index = 0;
    }

    // Inserting a top site pins it in the specified slot, pushing over any link already
    // pinned in the slot (unless it's the last slot, then it replaces).
    this._insertPin(
      action.data.site, index,
      action.data.draggedFromIndex !== undefined ? action.data.draggedFromIndex : this.store.getState().Prefs.values[ROWS_PREF] * TOP_SITES_MAX_SITES_PER_ROW);

    await this._clearLinkCustomScreenshot(action.data.site);
    this._broadcastPinnedSitesUpdated();
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.SYSTEM_TICK:
        this.refresh({broadcast: false});
        break;
      // All these actions mean we need new top sites
      case at.MIGRATION_COMPLETED:
      case at.PLACES_HISTORY_CLEARED:
      case at.PLACES_LINK_DELETED:
        this.frecentCache.expire();
        this.refresh({broadcast: true});
        break;
      case at.PLACES_LINKS_CHANGED:
        this.frecentCache.expire();
        this.refresh({broadcast: false});
        break;
      case at.PLACES_LINK_BLOCKED:
        this.frecentCache.expire();
        this.pinnedCache.expire();
        this.refresh({broadcast: true});
        break;
      case at.PREF_CHANGED:
        if (action.data.name === DEFAULT_SITES_PREF) {
          this.refreshDefaults(action.data.value);
        } else if (action.data.name === ROWS_PREF) {
          this.refresh({broadcast: true});
        }
        break;
      case at.UPDATE_SECTION_PREFS:
        if (action.data.id === SECTION_ID) {
          this.updateSectionPrefs(action.data.value);
        }
        break;
      case at.PREFS_INITIAL_VALUES:
        this.refreshDefaults(action.data[DEFAULT_SITES_PREF]);
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
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

this.DEFAULT_TOP_SITES = DEFAULT_TOP_SITES;
const EXPORTED_SYMBOLS = ["TopSitesFeed", "DEFAULT_TOP_SITES"];
