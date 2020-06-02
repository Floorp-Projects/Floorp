/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the export logins file picker appears.
 */

let { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

let { MockFilePicker } = SpecialPowers;

add_task(async function setup() {
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  }, "Waiting for content telemetry events to get cleared");

  MockFilePicker.init(window);
  MockFilePicker.useAnyFile();
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

function waitForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "Saw the file picker");
      resolve();
    };
  });
}

add_task(async function test_open_export() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async function(browser) {
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "menu-button",
        {},
        browser
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let menuButton = content.document.querySelector("menu-button");
        return ContentTaskUtils.waitForCondition(function waitForMenu() {
          return !menuButton.shadowRoot.querySelector(".menu").hidden;
        }, "waiting for menu to open");
      });

      function getExportMenuItem() {
        let menuButton = window.document.querySelector("menu-button");
        let exportButton = menuButton.shadowRoot.querySelector(
          ".menuitem-export"
        );
        // Force the menu item to be visible for the test.
        exportButton.hidden = false;
        return exportButton;
      }

      await BrowserTestUtils.synthesizeMouseAtCenter(
        getExportMenuItem,
        {},
        browser
      );

      // First event is for opening about:logins
      await LoginTestUtils.telemetry.waitForEventCount(2);
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "mgmt_menu_item_used", "export"]],
        { category: "pwmgr", method: "mgmt_menu_item_used" },
        { process: "content" }
      );

      info("Clicking confirm button");
      let filePicker = waitForFilePicker();
      await BrowserTestUtils.synthesizeMouseAtCenter(
        () => {
          let confirmExportDialog = window.document.querySelector(
            "confirmation-dialog"
          );
          return confirmExportDialog.shadowRoot.querySelector(
            ".confirm-button"
          );
        },
        {},
        browser
      );

      info("waiting for Export file picker to get opened");
      await filePicker;
      ok(true, "Export file picker opened");

      info("Waiting for the export to complete");
      await LoginTestUtils.telemetry.waitForEventCount(1, "parent");
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "mgmt_menu_item_used", "export_complete"]],
        { category: "pwmgr", method: "mgmt_menu_item_used" },
        { process: "parent" }
      );
    }
  );
});
