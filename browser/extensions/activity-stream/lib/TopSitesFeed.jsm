/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {TippyTopProvider} = Cu.import("resource://activity-stream/lib/TippyTopProvider.jsm", {});
const {insertPinned, TOP_SITES_SHOWMORE_LENGTH} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});
const {Dedupe} = Cu.import("resource://activity-stream/common/Dedupe.jsm", {});
const {shortURL} = Cu.import("resource://activity-stream/lib/ShortURL.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "filterAdult",
  "resource://activity-stream/lib/FilterAdult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm");

const UPDATE_TIME = 15 * 60 * 1000; // 15 minutes
const DEFAULT_SITES_PREF = "default.sites";
const DEFAULT_TOP_SITES = [];
const FRECENCY_THRESHOLD = 100; // 1 visit (skip first-run/one-time pages)
const MIN_FAVICON_SIZE = 96;

this.TopSitesFeed = class TopSitesFeed {
  constructor() {
    this.lastUpdated = 0;
    this._tippyTopProvider = new TippyTopProvider();
    this.dedupe = new Dedupe(this._dedupeKey);
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
  async getScreenshot(url) {
    let screenshot = await Screenshots.getScreenshotForURL(url);
    const action = {type: at.SCREENSHOT_UPDATED, data: {url, screenshot}};
    this.store.dispatch(ac.BroadcastToContent(action));
  }
  async getLinksWithDefaults(action) {
    // Get at least SHOWMORE amount so toggling between 1 and 2 rows has sites
    const numItems = Math.max(this.store.getState().Prefs.values.topSitesCount,
      TOP_SITES_SHOWMORE_LENGTH);
    let frecent = await NewTabUtils.activityStreamLinks.getTopSites({numItems});
    const notBlockedDefaultSites = DEFAULT_TOP_SITES.filter(site => !NewTabUtils.blockedLinks.isBlocked({url: site.url}));
    const defaultUrls = notBlockedDefaultSites.map(site => site.url);
    let pinned = this._getPinnedWithData(frecent);
    pinned = pinned.map(site => site && Object.assign({}, site, {
      isDefault: defaultUrls.indexOf(site.url) !== -1,
      hostname: shortURL(site)
    }));

    if (!frecent) {
      frecent = [];
    } else {
      // Get the best history links that pass the frecency threshold
      frecent = frecent.filter(link => link && link.type !== "affiliate" &&
        link.frecency > FRECENCY_THRESHOLD).map(site => {
          site.hostname = shortURL(site);
          return site;
        });
    }

    // Remove any duplicates from frecent and default sites
    const [, dedupedFrecent, dedupedDefaults] = this.dedupe.group(
      pinned, frecent, notBlockedDefaultSites);
    const dedupedUnpinned = [...dedupedFrecent, ...dedupedDefaults];

    // Remove adult sites if we need to
    const checkedAdult = this.store.getState().Prefs.values.filterAdult ?
      filterAdult(dedupedUnpinned) : dedupedUnpinned;

    // Insert the original pinned sites into the deduped frecent and defaults
    return insertPinned(checkedAdult, pinned).slice(0, numItems);
  }
  async refresh(target = null) {
    if (!this._tippyTopProvider.initialized) {
      await this._tippyTopProvider.init();
    }

    const links = await this.getLinksWithDefaults();

    // First, cache existing screenshots in case we need to reuse them
    const currentScreenshots = {};
    for (const link of this.store.getState().TopSites.rows) {
      if (link && link.screenshot) {
        currentScreenshots[link.url] = link.screenshot;
      }
    }

    // Now, get a tippy top icon, a rich icon, or screenshot for every item
    for (let link of links) {
      if (!link) { continue; }
      this._fetchIcon(link, currentScreenshots);
    }
    const newAction = {type: at.TOP_SITES_UPDATED, data: links};

    if (target) {
      // Send an update to content so the preloaded tab can get the updated content
      this.store.dispatch(ac.SendToContent(newAction, target));
    } else {
      // Broadcast an update to all open content pages
      this.store.dispatch(ac.BroadcastToContent(newAction));
    }
    this.lastUpdated = Date.now();
  }
  _fetchIcon(link, screenshotCache = {}) {
    // Check for tippy top icon or a rich icon.
    this._tippyTopProvider.processSite(link);
    if (!link.tippyTopIcon && (!link.favicon || link.faviconSize < MIN_FAVICON_SIZE)) {
      // If no tippy top, then we get a screenshot.
      if (screenshotCache[link.url]) {
        link.screenshot = screenshotCache[link.url];
      } else {
        this.getScreenshot(link.url);
      }
    }
  }
  _getPinnedWithData(links) {
    // Augment the pinned links with any other extra data we have for them already in the store.
    // Alternatively you can pass in some links that you know have data you want the pinned links
    // to also have. This is useful for start up to make sure pinned links have favicons
    // (See github ticket #3428 fore more details)
    const originalLinks = links || this.store.getState().TopSites.rows;
    const pinned = NewTabUtils.pinnedLinks.links;
    return pinned.map(pinnedLink => {
      if (pinnedLink) {
        const hostname = shortURL(pinnedLink);
        const originalLink = originalLinks.find(link => link && link.url === pinnedLink.url);
        // If it's a new link then it won't have an icon, so fetch one
        if (!originalLink) {
          this._fetchIcon(pinnedLink);
        }
        return Object.assign(originalLink || {hostname}, pinnedLink);
      }
      return pinnedLink;
    });
  }

  /**
   * Inform others that top sites data has been updated due to pinned changes.
   */
  _broadcastPinnedSitesUpdated() {
    // Refresh to update pinned sites with screenshots, trigger deduping, etc.
    this.refresh();
  }

  /**
   * Pin a site at a specific position saving only the desired keys.
   */
  _pinSiteAt({label, url}, index) {
    const toPin = {url};
    if (label) {
      toPin.label = label;
    }
    NewTabUtils.pinnedLinks.pin(toPin, index);
  }

  /**
   * Handle a pin action of a site to a position.
   */
  pin(action) {
    const {site, index} = action.data;
    this._pinSiteAt(site, index);
    this._broadcastPinnedSitesUpdated();
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
  _insertPin(site, index) {
    // For existing sites, recursively push it and others to the next positions
    let pinned = NewTabUtils.pinnedLinks.links;
    if (pinned.length > index && pinned[index]) {
      this._insertPin(pinned[index], index + 1);
    }
    this._pinSiteAt(site, index);
  }

  /**
   * Handle an add action of a site.
   */
  add(action) {
    // Adding a top site pins it in the first slot, pushing over any link already
    // pinned in the slot.
    this._insertPin(action.data.site, 0);
    this._broadcastPinnedSitesUpdated();
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.refresh();
        break;
      case at.NEW_TAB_LOAD:
        if (
          // When a new tab is opened, if the last time we refreshed the data
          // is greater than 15 minutes, refresh the data.
          (Date.now() - this.lastUpdated >= UPDATE_TIME)
        ) {
          this.refresh(action.meta.fromTarget);
        }
        break;
      // All these actions mean we need new top sites
      case at.MIGRATION_COMPLETED:
      case at.PLACES_HISTORY_CLEARED:
      case at.PLACES_LINK_DELETED:
      case at.PLACES_LINK_BLOCKED:
        this.refresh();
        break;
      case at.PREF_CHANGED:
        if (action.data.name === DEFAULT_SITES_PREF) {
          this.refreshDefaults(action.data.value);
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
      case at.TOP_SITES_ADD:
        this.add(action);
        break;
    }
  }
};

this.UPDATE_TIME = UPDATE_TIME;
this.DEFAULT_TOP_SITES = DEFAULT_TOP_SITES;
this.EXPORTED_SYMBOLS = ["TopSitesFeed", "UPDATE_TIME", "DEFAULT_TOP_SITES"];
