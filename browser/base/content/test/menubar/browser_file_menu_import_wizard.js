/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that if
  // we're using the new migration wizard, we'll load the about:preferences
  // page in a new tab rather than overtaking the initial one. This
  // makes it easier to be consistent with closing and opening
  // behaviours between the two kinds of migration wizards.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

add_task(async function file_menu_import_wizard() {
  // We can't call this code directly or our JS execution will get blocked on Windows/Linux where
  // the dialog is modal.
  executeSoon(() =>
    document.getElementById("menu_importFromAnotherBrowser").doCommand()
  );

  let wizard = await BrowserTestUtils.waitForMigrationWizard(window);
  ok(wizard, "Migrator window opened");
  await BrowserTestUtils.closeMigrationWizard(wizard);
});
