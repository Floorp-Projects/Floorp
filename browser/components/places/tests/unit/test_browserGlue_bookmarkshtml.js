/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly exports bookmarks.html at shutdown if
 * browser.bookmarks.autoExportHTML is set to true.
 */

function run_test() {
  run_next_test();
}

add_task(function* () {
  remove_bookmarks_html();

  Services.prefs.setBoolPref("browser.bookmarks.autoExportHTML", true);
  do_register_cleanup(() => Services.prefs.clearUserPref("browser.bookmarks.autoExportHTML"));

  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue);

  // Initialize Places through the History Service.
  Cc["@mozilla.org/browser/nav-history-service;1"]
    .getService(Ci.nsINavHistoryService);

  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "profile-before-change");
    check_bookmarks_html();
  }, "profile-before-change", false);
});
