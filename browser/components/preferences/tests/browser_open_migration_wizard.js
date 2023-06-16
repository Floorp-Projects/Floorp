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

      // First, we'll test the legacy Migration Wizard.
      await SpecialPowers.pushPrefEnv({
        set: [["browser.migrate.content-modal.enabled", false]],
      });

      let migrationWizardWindow = BrowserTestUtils.domWindowOpenedAndLoaded(
        null,
        win => {
          let type = win.document.documentElement.getAttribute("windowtype");
          if (type == "Browser:MigrationWizard") {
            Assert.ok(true, "Saw legacy Migration Wizard window open.");
            return true;
          }

          return false;
        }
      );

      button.click();
      let win = await migrationWizardWindow;
      await BrowserTestUtils.closeWindow(win);

      // Next, we'll test the new Migration Wizard.
      await SpecialPowers.pushPrefEnv({
        set: [["browser.migrate.content-modal.enabled", true]],
      });

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
