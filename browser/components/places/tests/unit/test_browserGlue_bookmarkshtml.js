/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly exports bookmarks.html at shutdown if
 * browser.bookmarks.autoExportHTML is set to true.
 */

add_task(async function() {
  remove_bookmarks_html();

  Services.prefs.setBoolPref("browser.bookmarks.autoExportHTML", true);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("browser.bookmarks.autoExportHTML")
  );

  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsISupports);

  // Initialize Places through the History Service.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE
  );

  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "profile-before-change");
    check_bookmarks_html();
  }, "profile-before-change");
});
