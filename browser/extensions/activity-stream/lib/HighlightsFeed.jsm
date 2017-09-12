/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

const {shortURL} = Cu.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {SectionsManager} = Cu.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {TOP_SITES_SHOWMORE_LENGTH} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});
const {Dedupe} = Cu.import("resource://activity-stream/common/Dedupe.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm");

const HIGHLIGHTS_MAX_LENGTH = 9;
const HIGHLIGHTS_UPDATE_TIME = 15 * 60 * 1000; // 15 minutes
const MANY_EXTRA_LENGTH = HIGHLIGHTS_MAX_LENGTH * 5 + TOP_SITES_SHOWMORE_LENGTH;
const SECTION_ID = "highlights";

this.HighlightsFeed = class HighlightsFeed {
  constructor() {
    this.highlightsLastUpdated = 0;
    this.highlights = [];
    this.dedupe = new Dedupe(this._dedupeKey);
    this.imageCache = new Map();
  }

  _dedupeKey(site) {
    return site && site.url;
  }

  init() {
    SectionsManager.onceInitialized(this.postInit.bind(this));
  }

  postInit() {
    SectionsManager.enableSection(SECTION_ID);
  }

  uninit() {
    SectionsManager.disableSection(SECTION_ID);
  }

  async fetchHighlights(broadcast = false) {
    // Request more than the expected length to allow for items being removed by
    // deduping against Top Sites or multiple history from the same domain, etc.
    const manyPages = await NewTabUtils.activityStreamLinks.getHighlights({numItems: MANY_EXTRA_LENGTH});

    // Remove any Highlights that are in Top Sites already
    const deduped = this.dedupe.group(this.store.getState().TopSites.rows, manyPages)[1];

    // Keep all "bookmark"s and at most one (most recent) "history" per host
    this.highlights = [];
    const hosts = new Set();
    for (const page of deduped) {
      const hostname = shortURL(page);
      // Skip this history page if we already something from the same host
      if (page.type === "history" && hosts.has(hostname)) {
        continue;
      }

      // If we already have the image for the card in the cache, use that
      // immediately. Then asynchronously fetch the image (refreshes the cache).
      const image = this.imageCache.get(page.url);
      this.fetchImage(page.url, page.preview_image_url);

      // We want the page, so update various fields for UI
      Object.assign(page, {
        image,
        hasImage: true, // We always have an image - fall back to a screenshot
        hostname,
        type: page.bookmarkGuid ? "bookmark" : page.type
      });

      // Add the "bookmark" or not-skipped "history"
      this.highlights.push(page);
      hosts.add(hostname);

      // Skip the rest if we have enough items
      if (this.highlights.length === HIGHLIGHTS_MAX_LENGTH) {
        break;
      }
    }

    SectionsManager.updateSection(SECTION_ID, {rows: this.highlights}, this.highlightsLastUpdated === 0 || broadcast);
    this.highlightsLastUpdated = Date.now();
    // Clearing the image cache here is okay. The asynchronous fetchImage calls
    // get scheduled after the body of fetchHighlights has been executed, so they
    // then fill up the cache again for the next fetchHighlights call.
    this.imageCache.clear();
  }

  /**
   * Fetch an image for a given highlight, store it in the image cache, and
   * update the card with the new image. If the highlight has a preview image
   * then use that, else fall back to a screenshot of the page.
   */
  async fetchImage(url, imageUrl) {
    const image = await Screenshots.getScreenshotForURL(imageUrl || url);
    if (image) {
      this.imageCache.set(url, image);
    }
    SectionsManager.updateSectionCard(SECTION_ID, url, {image}, true);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.NEW_TAB_LOAD:
        if (this.highlights.length < HIGHLIGHTS_MAX_LENGTH) {
          // If we haven't filled the highlights grid yet, fetch again.
          this.fetchHighlights(true);
        } else if (Date.now() - this.highlightsLastUpdated >= HIGHLIGHTS_UPDATE_TIME) {
          // If the last time we refreshed the data is greater than 15 minutes, fetch again.
          this.fetchHighlights(false);
        }
        break;
      case at.MIGRATION_COMPLETED:
      case at.PLACES_HISTORY_CLEARED:
      case at.PLACES_LINK_DELETED:
      case at.PLACES_LINK_BLOCKED:
        this.fetchHighlights(true);
        break;
      case at.PLACES_BOOKMARK_ADDED:
      case at.PLACES_BOOKMARK_REMOVED:
      case at.TOP_SITES_UPDATED:
        this.fetchHighlights(false);
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

this.HIGHLIGHTS_UPDATE_TIME = HIGHLIGHTS_UPDATE_TIME;
this.EXPORTED_SYMBOLS = ["HighlightsFeed", "HIGHLIGHTS_UPDATE_TIME", "SECTION_ID"];
