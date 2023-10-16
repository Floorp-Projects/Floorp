/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the "Import Data" button in the "Import Browser Data" section of
 * the General pane of about:preferences launches the Migration Wizard.
 */
add_task(async function test_open_migration_wizard() {
  const BUTTON_ID = "data-migration";

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#general" },
    async function (browser) {
      let button = browser.contentDocument.getElementById(BUTTON_ID);

      let wizardReady = BrowserTestUtils.waitForEvent(
        browser.contentWindow,
        "MigrationWizard:Ready"
      );
      button.click();
      await wizardReady;
      Assert.ok(true, "Saw the new Migration Wizard dialog open.");
    }
  );
});
