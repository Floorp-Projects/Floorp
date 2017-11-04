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

XPCOMUtils.defineLazyModuleGetter(this, "filterAdult",
  "resource://activity-stream/lib/FilterAdult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LinksCache",
  "resource://activity-stream/lib/LinksCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm");

const HIGHLIGHTS_MAX_LENGTH = 9;
const MANY_EXTRA_LENGTH = HIGHLIGHTS_MAX_LENGTH * 5 + TOP_SITES_SHOWMORE_LENGTH;
const SECTION_ID = "highlights";

this.HighlightsFeed = class HighlightsFeed {
  constructor() {
    this.dedupe = new Dedupe(this._dedupeKey);
    this.linksCache = new LinksCache(NewTabUtils.activityStreamLinks,
      "getHighlights", ["image"]);
  }

  _dedupeKey(site) {
    // Treat bookmarks as un-dedupable, otherwise show one of a url
    return site && (site.type === "bookmark" ? {} : site.url);
  }

  init() {
    SectionsManager.onceInitialized(this.postInit.bind(this));
  }

  postInit() {
    SectionsManager.enableSection(SECTION_ID);
    this.fetchHighlights({broadcast: true});
  }

  uninit() {
    SectionsManager.disableSection(SECTION_ID);
  }

  /**
   * Refresh the highlights data for content.
   * @param {bool} options.broadcast Should the update be broadcasted.
   */
  async fetchHighlights(options = {}) {
    // We need TopSites for deduping, so wait for TOP_SITES_UPDATED.
    if (!this.store.getState().TopSites.initialized) {
      return;
    }

    // We broadcast when we want to force an update, so get fresh links
    if (options.broadcast) {
      this.linksCache.expire();
    }

    // Request more than the expected length to allow for items being removed by
    // deduping against Top Sites or multiple history from the same domain, etc.
    const manyPages = await this.linksCache.request({numItems: MANY_EXTRA_LENGTH});

    // Remove adult highlights if we need to
    const checkedAdult = this.store.getState().Prefs.values.filterAdult ?
      filterAdult(manyPages) : manyPages;

    // Remove any Highlights that are in Top Sites already
    const [, deduped] = this.dedupe.group(this.store.getState().TopSites.rows, checkedAdult);

    // Keep all "bookmark"s and at most one (most recent) "history" per host
    const highlights = [];
    const hosts = new Set();
    for (const page of deduped) {
      const hostname = shortURL(page);
      // Skip this history page if we already something from the same host
      if (page.type === "history" && hosts.has(hostname)) {
        continue;
      }

      // If we already have the image for the card, use that immediately. Else
      // asynchronously fetch the image.
      if (!page.image) {
        this.fetchImage(page);
      }

      // We want the page, so update various fields for UI
      Object.assign(page, {
        hasImage: true, // We always have an image - fall back to a screenshot
        hostname,
        type: page.bookmarkGuid ? "bookmark" : page.type
      });

      // Add the "bookmark" or not-skipped "history"
      highlights.push(page);
      hosts.add(hostname);

      // Remove internal properties that might be updated after dispatch
      delete page.__sharedCache;

      // Skip the rest if we have enough items
      if (highlights.length === HIGHLIGHTS_MAX_LENGTH) {
        break;
      }
    }

    const sectionIndex = SectionsManager.sections.get(SECTION_ID).order;
    const {initialized} = this.store.getState().Sections[sectionIndex];
    // Broadcast when required or if it is the first update.
    const shouldBroadcast = options.broadcast || !initialized;

    SectionsManager.updateSection(SECTION_ID, {rows: highlights}, shouldBroadcast);
  }

  /**
   * Fetch an image for a given highlight and update the card with it. If no
   * image is available then fallback to fetching a screenshot.
   */
  async fetchImage(page) {
    // Request a screenshot if we don't already have one pending
    const {preview_image_url: imageUrl, url} = page;
    Screenshots.maybeCacheScreenshot(page, imageUrl || url, "image", image => {
      SectionsManager.updateSectionCard(SECTION_ID, url, {image}, true);
    });
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.SYSTEM_TICK:
        this.fetchHighlights({broadcast: false});
        break;
      case at.MIGRATION_COMPLETED:
      case at.PLACES_HISTORY_CLEARED:
      case at.PLACES_LINKS_DELETED:
      case at.PLACES_LINK_BLOCKED:
        this.fetchHighlights({broadcast: true});
        break;
      case at.PLACES_BOOKMARK_ADDED:
      case at.PLACES_BOOKMARK_REMOVED:
        this.linksCache.expire();
        this.fetchHighlights({broadcast: false});
        break;
      case at.TOP_SITES_UPDATED:
        this.fetchHighlights({broadcast: false});
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["HighlightsFeed", "SECTION_ID"];
