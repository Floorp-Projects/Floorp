/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

const {shortURL} = Cu.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {SectionsManager} = Cu.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {Dedupe} = Cu.import("resource://activity-stream/common/Dedupe.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");

const HIGHLIGHTS_MAX_LENGTH = 9;
const HIGHLIGHTS_UPDATE_TIME = 15 * 60 * 1000; // 15 minutes
const SECTION_ID = "highlights";

this.HighlightsFeed = class HighlightsFeed {
  constructor() {
    this.highlightsLastUpdated = 0;
    this.highlights = [];
    this.dedupe = new Dedupe(this._dedupeKey);
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
    this.highlights = await NewTabUtils.activityStreamLinks.getHighlights();
    for (let highlight of this.highlights) {
      highlight.hostname = shortURL(Object.assign({}, highlight, {url: highlight.url}));
      highlight.image = highlight.preview_image_url;
      if (highlight.bookmarkGuid) {
        highlight.type = "bookmark";
      }
    }

    // Remove any Highlights that are in Top Sites already
    const deduped = this.dedupe.group(this.store.getState().TopSites.rows, this.highlights);
    this.highlights = deduped[1];

    SectionsManager.updateSection(SECTION_ID, {rows: this.highlights}, this.highlightsLastUpdated === 0 || broadcast);
    this.highlightsLastUpdated = Date.now();
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
