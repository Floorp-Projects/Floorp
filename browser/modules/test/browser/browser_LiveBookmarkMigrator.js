/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

// Check migrator can open the SUMO page.
add_task(async function() {
  let expectedURL =
    Services.prefs.getCharPref("app.support.baseURL") +
    "live-bookmarks-migration";
  let newTabExpected = BrowserTestUtils.waitForNewTab(
    gBrowser,
    expectedURL,
    true
  );
  let { LiveBookmarkMigrator } = ChromeUtils.import(
    "resource:///modules/LiveBookmarkMigrator.jsm"
  );
  LiveBookmarkMigrator._openSUMOPage();
  await newTabExpected;
  // If we get here, this is guaranteed to pass, but otherwise mochitest complains that
  // the test has no assertions.
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedURL,
    "Should have opened the right page"
  );
  gBrowser.removeCurrentTab();
});
