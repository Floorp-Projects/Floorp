/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that when the
  // about:preferences hosting the migration wizard opens, we'll load
  // the about:preferences page in a new tab rather than overtaking the
  // initial one. This makes it easier to be more explicit when cleaning
  // up because we can just remove the opened tab.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

add_task(async function file_menu_import_wizard() {
  let wizardTabPromise = BrowserTestUtils.waitForMigrationWizard(window);
  document.getElementById("menu_importFromAnotherBrowser").doCommand();
  let wizardTab = await wizardTabPromise;
  ok(wizardTab, "Migration wizard tab opened");
  BrowserTestUtils.removeTab(wizardTab);
});
