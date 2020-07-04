/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);

let { MockFilePicker } = SpecialPowers;

/**
 * Waits until the mock file picker is opened and sets the destFilePath as it's selected file.
 *
 * @param {nsIFile} destFile
 *        The file being passed to the picker.
 * @returns {string} A promise that is resolved when the picker selects the file.
 */
function waitForOpenFilePicker(destFile) {
  return new Promise(resolve => {
    MockFilePicker.showCallback = function(fp) {
      info("showCallback");
      info("fileName: " + destFile.path);
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 1;
      info("done showCallback");
      resolve();
    };
  });
}

add_task(async function test_open_import_from_csv() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async function(browser) {
      MockFilePicker.init(window);
      MockFilePicker.returnValue = MockFilePicker.returnOK;

      let csvFile = await LoginTestUtils.file.setupCsvFileWithLines([
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        "https://example.com,joe@example.com,qwerty,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802",
      ]);
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

      function getImportMenuItem() {
        let menuButton = window.document.querySelector("menu-button");
        let importButton = menuButton.shadowRoot.querySelector(
          ".menuitem-import-file"
        );
        // Force the menu item to be visible for the test.
        importButton.hidden = false;
        return importButton;
      }

      EXPECTED_ERROR_MESSAGE = "Couldn't parse origin for";
      Services.telemetry.clearEvents();

      let filePicker = waitForOpenFilePicker(csvFile);
      await BrowserTestUtils.synthesizeMouseAtCenter(
        getImportMenuItem,
        {},
        browser
      );

      // First event is for opening about:logins
      await LoginTestUtils.telemetry.waitForEventCount(
        1,
        "content",
        "pwmgr",
        "mgmt_menu_item_used"
      );
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "mgmt_menu_item_used", "import_from_csv"]],
        { category: "pwmgr", method: "mgmt_menu_item_used" },
        { process: "content", clear: false }
      );

      info("waiting for Import file picker to get opened");
      await filePicker;
      ok(true, "Import file picker opened");
      info("Waiting for the import to complete");
      await LoginTestUtils.telemetry.waitForEventCount(1, "parent");
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "mgmt_menu_item_used", "import_csv_complete"]],
        { category: "pwmgr", method: "mgmt_menu_item_used" },
        { process: "parent" }
      );

      EXPECTED_ERROR_MESSAGE = null;
    }
  );
});
