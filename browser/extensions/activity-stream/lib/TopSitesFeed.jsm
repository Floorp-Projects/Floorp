/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {insertPinned} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
// Keep a reference to PreviewProvider.jsm until it's good to remove. See #2849
XPCOMUtils.defineLazyModuleGetter(this, "PreviewProvider",
  "resource://app/modules/PreviewProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm");

const TOP_SITES_SHOWMORE_LENGTH = 12;
const UPDATE_TIME = 15 * 60 * 1000; // 15 minutes
const DEFAULT_TOP_SITES = [];

this.TopSitesFeed = class TopSitesFeed {
  constructor() {
    this.lastUpdated = 0;
  }
  init() {
    // Add default sites if any based on the pref
    let sites = new Prefs().get("default.sites");
    if (sites) {
      for (const url of sites.split(",")) {
        DEFAULT_TOP_SITES.push({
          isDefault: true,
          url
        });
      }
    }
  }
  async getScreenshot(url) {
    let screenshot = await Screenshots.getScreenshotForURL(url);
    const action = {type: at.SCREENSHOT_UPDATED, data: {url, screenshot}};
    this.store.dispatch(ac.BroadcastToContent(action));
  }
  async getLinksWithDefaults(action) {
    let frecent = await NewTabUtils.activityStreamLinks.getTopSites();
    const defaultUrls = DEFAULT_TOP_SITES.map(site => site.url);
    let pinned = NewTabUtils.pinnedLinks.links;
    pinned = pinned.map(site => site && Object.assign({}, site, {isDefault: defaultUrls.indexOf(site.url) !== -1}));

    if (!frecent) {
      frecent = [];
    } else {
      frecent = frecent.filter(link => link && link.type !== "affiliate");
    }

    return insertPinned([...frecent, ...DEFAULT_TOP_SITES], pinned).slice(0, TOP_SITES_SHOWMORE_LENGTH);
  }
  async refresh(target = null) {
    const links = await this.getLinksWithDefaults();

    // First, cache existing screenshots in case we need to reuse them
    const currentScreenshots = {};
    for (const link of this.store.getState().TopSites.rows) {
      if (link && link.screenshot) {
        currentScreenshots[link.url] = link.screenshot;
      }
    }

    // Now, get a screenshot for every item
    for (let link of links) {
      if (!link) { continue; }
      if (currentScreenshots[link.url]) {
        link.screenshot = currentScreenshots[link.url];
      } else {
        this.getScreenshot(link.url);
      }
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
  _getPinnedWithData() {
    // Augment the pinned links with any other extra data we have for them already in the store
    const links = this.store.getState().TopSites.rows;
    const pinned = NewTabUtils.pinnedLinks.links;
    return pinned.map(pinnedLink => (pinnedLink ? Object.assign(links.find(link => link && link.url === pinnedLink.url) || {}, pinnedLink) : pinnedLink));
  }
  pin(action) {
    const {site, index} = action.data;
    NewTabUtils.pinnedLinks.pin(site, index);
    this.store.dispatch(ac.BroadcastToContent({
      type: at.PINNED_SITES_UPDATED,
      data: this._getPinnedWithData()
    }));
  }
  unpin(action) {
    const {site} = action.data;
    NewTabUtils.pinnedLinks.unpin(site);
    this.store.dispatch(ac.BroadcastToContent({
      type: at.PINNED_SITES_UPDATED,
      data: this._getPinnedWithData()
    }));
  }
  onAction(action) {
    let realRows;
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.NEW_TAB_LOAD:
        // Only check against real rows returned from history, not default ones.
        realRows = this.store.getState().TopSites.rows.filter(row => !row.isDefault);
        if (
          // When a new tab is opened, if we don't have enough top sites yet, refresh the data.
          (realRows.length < TOP_SITES_SHOWMORE_LENGTH) ||

          // When a new tab is opened, if the last time we refreshed the data
          // is greater than 15 minutes, refresh the data.
          (Date.now() - this.lastUpdated >= UPDATE_TIME)
        ) {
          this.refresh(action.meta.fromTarget);
        }
        break;
      case at.PLACES_HISTORY_CLEARED:
        this.refresh();
        break;
      case at.TOP_SITES_PIN:
        this.pin(action);
        break;
      case at.TOP_SITES_UNPIN:
        this.unpin(action);
        break;
    }
  }
};

this.UPDATE_TIME = UPDATE_TIME;
this.TOP_SITES_SHOWMORE_LENGTH = TOP_SITES_SHOWMORE_LENGTH;
this.DEFAULT_TOP_SITES = DEFAULT_TOP_SITES;
this.EXPORTED_SYMBOLS = ["TopSitesFeed", "UPDATE_TIME", "DEFAULT_TOP_SITES", "TOP_SITES_SHOWMORE_LENGTH"];
