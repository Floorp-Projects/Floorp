/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the export logins file picker appears.
 */

let { MockFilePicker } = SpecialPowers;

add_task(async function setup() {
  MockFilePicker.init(window);
  MockFilePicker.returnValue = MockFilePicker.returnCancel;
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

      let filePicker = waitForFilePicker();
      await BrowserTestUtils.synthesizeMouseAtCenter(
        getExportMenuItem,
        {},
        browser
      );

      info("Clicking confirm button");
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
    }
  );
});
