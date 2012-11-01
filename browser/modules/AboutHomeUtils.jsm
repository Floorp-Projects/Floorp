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
const STARTPAGE_VERSION = 3;

this.AboutHomeUtils = new Object();

/**
 * Returns an object containing the name and searchURL of the original default
 * search engine.
 */
XPCOMUtils.defineLazyGetter(AboutHomeUtils, "defaultSearchEngine", function() {
  let defaultEngine = Services.search.originalDefaultEngine;
  let submission = defaultEngine.getSubmission("_searchTerms_");
  if (submission.postData) {
    throw new Error("Home page does not support POST search engines.");
  }

  return Object.freeze({
    name: defaultEngine.name,
    searchURL: submission.uri.spec
  });
});

/**
 * Returns the URL to fetch snippets from, in the urlFormatter service format.
 */
XPCOMUtils.defineLazyGetter(AboutHomeUtils, "snippetsURL", function() {
  let updateURL = Services.prefs
                          .getCharPref(SNIPPETS_URL_PREF)
                          .replace("%STARTPAGE_VERSION%", STARTPAGE_VERSION);
  return Services.urlFormatter.formatURL(updateURL);
});
