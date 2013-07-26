/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "AboutHomeUtils" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

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
