/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["LaterRun"];

Cu.import("resource://gre/modules/Services.jsm");

const kEnabledPref = "browser.laterrun.enabled";
const kPagePrefRoot = "browser.laterrun.pages.";
// Number of sessions we've been active in
const kSessionCountPref = "browser.laterrun.bookkeeping.sessionCount";
// Time the profile was created at:
const kProfileCreationTime = "browser.laterrun.bookkeeping.profileCreationTime";

// After 50 sessions or 1 month since install, assume we will no longer be
// interested in showing anything to "new" users
const kSelfDestructSessionLimit = 50;
const kSelfDestructHoursLimit = 31 * 24;

class Page {
  constructor({pref, minimumHoursSinceInstall, minimumSessionCount, requireBoth, url}) {
    this.pref = pref;
    this.minimumHoursSinceInstall = minimumHoursSinceInstall || 0;
    this.minimumSessionCount = minimumSessionCount || 1;
    this.requireBoth = requireBoth || false;
    this.url = url;
  }

  get hasRun() {
    return Services.prefs.getBoolPref(this.pref + "hasRun", false);
  }

  applies(sessionInfo) {
    if (this.hasRun) {
      return false;
    }
    if (this.requireBoth) {
      return sessionInfo.sessionCount >= this.minimumSessionCount &&
             sessionInfo.hoursSinceInstall >= this.minimumHoursSinceInstall;
    }
    return sessionInfo.sessionCount >= this.minimumSessionCount ||
           sessionInfo.hoursSinceInstall >= this.minimumHoursSinceInstall;
  }
}

let LaterRun = {
  init() {
    if (!this.enabled) {
      return;
    }
    // If this is the first run, set the time we were installed
    if (Services.prefs.getPrefType(kProfileCreationTime) == Ci.nsIPrefBranch.PREF_INVALID) {
      // We need to store seconds in order to fit within int prefs.
      Services.prefs.setIntPref(kProfileCreationTime, Math.floor(Date.now() / 1000));
    }
    this.sessionCount++;

    if (this.hoursSinceInstall > kSelfDestructHoursLimit ||
        this.sessionCount > kSelfDestructSessionLimit) {
      this.selfDestruct();
    }
  },

  // The enabled, hoursSinceInstall and sessionCount properties mirror the
  // preferences system, and are here for convenience.
  get enabled() {
    return Services.prefs.getBoolPref(kEnabledPref, false);
  },

  set enabled(val) {
    let wasEnabled = this.enabled;
    Services.prefs.setBoolPref(kEnabledPref, val);
    if (val && !wasEnabled) {
      this.init();
    }
  },

  get hoursSinceInstall() {
    let installStamp = Services.prefs.getIntPref(kProfileCreationTime, Date.now() / 1000);
    return Math.floor((Date.now() / 1000 - installStamp) / 3600);
  },

  get sessionCount() {
    if (this._sessionCount) {
      return this._sessionCount;
    }
    return this._sessionCount = Services.prefs.getIntPref(kSessionCountPref, 0);
  },

  set sessionCount(val) {
    this._sessionCount = val;
    Services.prefs.setIntPref(kSessionCountPref, val);
  },

  // Because we don't want to keep incrementing this indefinitely for no reason,
  // we will turn ourselves off after a set amount of time/sessions (see top of
  // file).
  selfDestruct() {
    Services.prefs.setBoolPref(kEnabledPref, false);
  },

  // Create an array of Page objects based on the currently set prefs
  readPages() {
    // Enumerate all the pages.
    let allPrefsForPages = Services.prefs.getChildList(kPagePrefRoot);
    let pageDataStore = new Map();
    for (let pref of allPrefsForPages) {
      let [slug, prop] = pref.substring(kPagePrefRoot.length).split(".");
      if (!pageDataStore.has(slug)) {
        pageDataStore.set(slug, {pref: pref.substring(0, pref.length - prop.length)});
      }
      if (prop == "requireBoth" || prop == "hasRun") {
        pageDataStore.get(slug)[prop] = Services.prefs.getBoolPref(pref, false);
      } else if (prop == "url") {
        pageDataStore.get(slug)[prop] = Services.prefs.getStringPref(pref, "");
      } else {
        pageDataStore.get(slug)[prop] = Services.prefs.getIntPref(pref, 0);
      }
    }
    let rv = [];
    for (let [, pageData] of pageDataStore) {
      if (pageData.url) {
        let uri = null;
        try {
          let urlString = Services.urlFormatter.formatURL(pageData.url.trim());
          uri = Services.io.newURI(urlString);
        } catch (ex) {
          Cu.reportError("Invalid LaterRun page URL " + pageData.url + " ignored.");
          continue;
        }
        if (!uri.schemeIs("https")) {
          Cu.reportError("Insecure LaterRun page URL " + uri.spec + " ignored.");
        } else {
          pageData.url = uri.spec;
          rv.push(new Page(pageData));
        }
      }
    }
    return rv;
  },

  // Return a URL for display as a 'later run' page if its criteria are matched,
  // or null otherwise.
  // NB: will only return one page at a time; if multiple pages match, it's up
  // to the preference service which one gets shown first, and the next one
  // will be shown next startup instead.
  getURL() {
    if (!this.enabled) {
      return null;
    }
    let pages = this.readPages();
    let page = pages.find(p => p.applies(this));
    if (page) {
      Services.prefs.setBoolPref(page.pref + "hasRun", true);
      return page.url;
    }
    return null;
  },
};

LaterRun.init();
