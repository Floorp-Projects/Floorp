/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PreviewProvider",
  "resource:///modules/PreviewProvider.jsm");

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
    let screenshot = await PreviewProvider.getThumbnail(url);
    const action = {type: at.SCREENSHOT_UPDATED, data: {url, screenshot}};
    this.store.dispatch(ac.BroadcastToContent(action));
  }
  sortLinks(frecent, pinned) {
    let sortedLinks = [...frecent, ...DEFAULT_TOP_SITES];
    sortedLinks = sortedLinks.filter(link => !NewTabUtils.pinnedLinks.isPinned(link));

    // Insert the pinned links in their specified location
    pinned.forEach((val, index) => {
      if (!val) { return; }
      let link = Object.assign({}, val, {isPinned: true, pinIndex: index, pinTitle: val.title});
      if (index > sortedLinks.length) {
        sortedLinks[index] = link;
      } else {
        sortedLinks.splice(index, 0, link);
      }
    });

    return sortedLinks.slice(0, TOP_SITES_SHOWMORE_LENGTH);
  }
  async getLinksWithDefaults(action) {
    let pinned = NewTabUtils.pinnedLinks.links;
    let frecent = await NewTabUtils.activityStreamLinks.getTopSites();

    if (!frecent) {
      frecent = [];
    } else {
      frecent = frecent.filter(link => link && link.type !== "affiliate");
    }

    return this.sortLinks(frecent, pinned);
  }
  async refresh(action) {
    const links = await this.getLinksWithDefaults();

    // First, cache existing screenshots in case we need to reuse them
    const currentScreenshots = {};
    for (const link of this.store.getState().TopSites.rows) {
      if (link.screenshot) {
        currentScreenshots[link.url] = link.screenshot;
      }
    }

    // Now, get a screenshot for every item
    for (let link of links) {
      if (currentScreenshots[link.url]) {
        link.screenshot = currentScreenshots[link.url];
      } else {
        this.getScreenshot(link.url);
      }
    }

    const newAction = {type: at.TOP_SITES_UPDATED, data: links};

    // Send an update to content so the preloaded tab can get the updated content
    this.store.dispatch(ac.SendToContent(newAction, action.meta.fromTarget));
    this.lastUpdated = Date.now();
  }
  openNewWindow(action, isPrivate = false) {
    const win = action._target.browser.ownerGlobal;
    win.openLinkIn(action.data.url, "window", {private: isPrivate});
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
          this.refresh(action);
        }
        break;
      case at.OPEN_NEW_WINDOW:
        this.openNewWindow(action);
        break;
      case at.OPEN_PRIVATE_WINDOW: {
        this.openNewWindow(action, true);
        break;
      }
    }
  }
};

this.UPDATE_TIME = UPDATE_TIME;
this.TOP_SITES_SHOWMORE_LENGTH = TOP_SITES_SHOWMORE_LENGTH;
this.DEFAULT_TOP_SITES = DEFAULT_TOP_SITES;
this.EXPORTED_SYMBOLS = ["TopSitesFeed", "UPDATE_TIME", "DEFAULT_TOP_SITES", "TOP_SITES_SHOWMORE_LENGTH"];
