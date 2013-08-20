/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AboutHomeUtils", "AboutHome" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// Url to fetch snippets, in the urlFormatter service format.
const SNIPPETS_URL_PREF = "browser.aboutHomeSnippets.updateUrl";

// Should be bumped up if the snippets content format changes.
const STARTPAGE_VERSION = 4;

this.AboutHomeUtils = {
  get snippetsVersion() STARTPAGE_VERSION,

  /**
   * Returns an object containing the name and searchURL of the original default
   * search engine.
   */
  get defaultSearchEngine() {
    let defaultEngine = Services.search.defaultEngine;
    let submission = defaultEngine.getSubmission("_searchTerms_", null, "homepage");

    return Object.freeze({
      name: defaultEngine.name,
      searchURL: submission.uri.spec,
      postDataString: submission.postDataString
    });
  },

  /*
   * showKnowYourRights - Determines if the user should be shown the
   * about:rights notification. The notification should *not* be shown if
   * we've already shown the current version, or if the override pref says to
   * never show it. The notification *should* be shown if it's never been seen
   * before, if a newer version is available, or if the override pref says to
   * always show it.
   */
  get showKnowYourRights() {
    // Look for an unconditional override pref. If set, do what it says.
    // (true --> never show, false --> always show)
    try {
      return !Services.prefs.getBoolPref("browser.rights.override");
    } catch (e) { }
    // Ditto, for the legacy EULA pref.
    try {
      return !Services.prefs.getBoolPref("browser.EULA.override");
    } catch (e) { }

#ifndef MOZILLA_OFFICIAL
    // Non-official builds shouldn't show the notification.
    return false;
#endif

    // Look to see if the user has seen the current version or not.
    var currentVersion = Services.prefs.getIntPref("browser.rights.version");
    try {
      return !Services.prefs.getBoolPref("browser.rights." + currentVersion + ".shown");
    } catch (e) { }

    // Legacy: If the user accepted a EULA, we won't annoy them with the
    // equivalent about:rights page until the version changes.
    try {
      return !Services.prefs.getBoolPref("browser.EULA." + currentVersion + ".accepted");
    } catch (e) { }

    // We haven't shown the notification before, so do so now.
    return true;
  }
};

/**
 * Returns the URL to fetch snippets from, in the urlFormatter service format.
 */
XPCOMUtils.defineLazyGetter(AboutHomeUtils, "snippetsURL", function() {
  let updateURL = Services.prefs
                          .getCharPref(SNIPPETS_URL_PREF)
                          .replace("%STARTPAGE_VERSION%", STARTPAGE_VERSION);
  return Services.urlFormatter.formatURL(updateURL);
});

/**
 * This code provides services to the about:home page. Whenever
 * about:home needs to do something chrome-privileged, it sends a
 * message that's handled here.
 */
let AboutHome = {
  MESSAGES: [
    "AboutHome:RestorePreviousSession",
    "AboutHome:Downloads",
    "AboutHome:Bookmarks",
    "AboutHome:History",
    "AboutHome:Apps",
    "AboutHome:Addons",
    "AboutHome:Sync",
    "AboutHome:Settings",
    "AboutHome:RequestUpdate",
    "AboutHome:Search",
  ],

  init: function() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);

    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
  },

  observe: function(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case "browser-search-engine-modified":
        this.sendAboutHomeData(null);
        break;
    }
  },

  receiveMessage: function(aMessage) {
    let window = aMessage.target.ownerDocument.defaultView;

    switch (aMessage.name) {
      case "AboutHome:RestorePreviousSession":
        let ss = Cc["@mozilla.org/browser/sessionstore;1"].
                 getService(Ci.nsISessionStore);
        if (ss.canRestoreLastSession) {
          ss.restoreLastSession();
        }
        break;

      case "AboutHome:Downloads":
        window.BrowserDownloadsUI();
        break;

      case "AboutHome:Bookmarks":
        window.PlacesCommandHook.showPlacesOrganizer("AllBookmarks");
        break;

      case "AboutHome:History":
        window.PlacesCommandHook.showPlacesOrganizer("History");
        break;

      case "AboutHome:Apps":
        window.openUILinkIn("https://marketplace.mozilla.org/", "tab");
        break;

      case "AboutHome:Addons":
        window.BrowserOpenAddonsMgr();
        break;

      case "AboutHome:Sync":
        window.openPreferences("paneSync");
        break;

      case "AboutHome:Settings":
        window.openPreferences();
        break;

      case "AboutHome:RequestUpdate":
        this.sendAboutHomeData(aMessage.target);
        break;

      case "AboutHome:Search":
#ifdef MOZ_SERVICES_HEALTHREPORT
        window.BrowserSearch.recordSearchInHealthReport(aMessage.data.engineName, "abouthome");
#endif
        break;
    }
  },

  // Send all the chrome-privileged data needed by about:home. This
  // gets re-sent when the search engine changes.
  sendAboutHomeData: function(target) {
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].
               getService(Ci.nsISessionStore);
    let data = {
      showRestoreLastSession: ss.canRestoreLastSession,
      snippetsURL: AboutHomeUtils.snippetsURL,
      showKnowYourRights: AboutHomeUtils.showKnowYourRights,
      snippetsVersion: AboutHomeUtils.snippetsVersion,
      defaultSearchEngine: AboutHomeUtils.defaultSearchEngine
    };

    if (AboutHomeUtils.showKnowYourRights) {
      // Set pref to indicate we've shown the notification.
      let currentVersion = Services.prefs.getIntPref("browser.rights.version");
      Services.prefs.setBoolPref("browser.rights." + currentVersion + ".shown", true);
    }

    if (target) {
      target.messageManager.sendAsyncMessage("AboutHome:Update", data);
    } else {
      let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
      mm.broadcastAsyncMessage("AboutHome:Update", data);
    }
  },
};
