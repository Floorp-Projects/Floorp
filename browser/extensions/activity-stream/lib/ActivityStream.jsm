/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals XPCOMUtils, NewTabInit, TopSitesFeed, SearchFeed */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const {Store} = Cu.import("resource://activity-stream/lib/Store.jsm", {});
const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

// Feeds
XPCOMUtils.defineLazyModuleGetter(this, "NewTabInit",
  "resource://activity-stream/lib/NewTabInit.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TopSitesFeed",
  "resource://activity-stream/lib/TopSitesFeed.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SearchFeed",
  "resource://activity-stream/lib/SearchFeed.jsm");

const feeds = {
  // When you add a feed here:
  // 1. The key in this object should directly refer to a pref, not including the
  //    prefix (so "feeds.newtabinit" refers to the
  //    "browser.newtabpage.activity-stream.feeds.newtabinit" pref)
  // 2. The value should be a function that returns a feed.
  // 3. You should use XPCOMUtils.defineLazyModuleGetter to import the Feed,
  //    so it isn't loaded until the feed is enabled.
  "feeds.newtabinit": () => new NewTabInit(),
  "feeds.topsites": () => new TopSitesFeed(),
  "feeds.search": () => new SearchFeed()
};

this.ActivityStream = class ActivityStream {

  /**
   * constructor - Initializes an instance of ActivityStream
   *
   * @param  {object} options Options for the ActivityStream instance
   * @param  {string} options.id Add-on ID. e.g. "activity-stream@mozilla.org".
   * @param  {string} options.version Version of the add-on. e.g. "0.1.0"
   * @param  {string} options.newTabURL URL of New Tab page on which A.S. is displayed. e.g. "about:newtab"
   */
  constructor(options) {
    this.initialized = false;
    this.options = options;
    this.store = new Store();
    this.feeds = feeds;
  }
  init() {
    this.initialized = true;
    this.store.init(this.feeds);
    this.store.dispatch({type: at.INIT});
  }
  uninit() {
    this.store.dispatch({type: at.UNINIT});
    this.store.uninit();
    this.initialized = false;
  }
};

this.EXPORTED_SYMBOLS = ["ActivityStream"];
