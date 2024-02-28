/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);

let { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

let { MockFilePicker } = SpecialPowers;

/**
 * A helper class to deal with Login CSV import UI.
 */
class CsvImportHelper {
  /**
   * Waits until the mock file picker is opened and sets the destFilePath as it's selected file.
   *
   * @param {nsIFile} destFile
   *        The file being passed to the picker.
   * @returns {string} A promise that is resolved when the picker selects the file.
   */
  static waitForOpenFilePicker(destFile) {
    return new Promise(resolve => {
      MockFilePicker.showCallback = fp => {
        info("showCallback");
        info("fileName: " + destFile.path);
        MockFilePicker.setFiles([destFile]);
        MockFilePicker.filterIndex = 1;
        info("done showCallback");
        resolve();
      };
    });
  }

  /**
   * Clicks the 3 dot menu and then "Import from a file..." and then it serves a CSV file.
   * It also does the needed assertions and telemetry validations.
   * If you await for it to return, it will have processed the CSV file already.
   *
   * @param {browser} browser
   *        The browser object.
   * @param {string[]} linesInFile
   *        An array of strings to be used to generate the CSV file. Each string is a line.
   * @returns {Promise} A promise that is resolved when the picker selects the file.
   */
  static async clickImportFromCsvMenu(browser, linesInFile) {
    MockFilePicker.init(window);
    MockFilePicker.returnValue = MockFilePicker.returnOK;
    let csvFile = await LoginTestUtils.file.setupCsvFileWithLines(linesInFile);

    await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);

    await SpecialPowers.spawn(browser, [], async () => {
      let menuButton = content.document.querySelector("menu-button");
      return ContentTaskUtils.waitForCondition(function waitForMenu() {
        return !menuButton.shadowRoot.querySelector(".menu").hidden;
      }, "waiting for menu to open");
    });

    Services.telemetry.clearEvents();

    function getImportMenuItem() {
      let menuButton = window.document.querySelector("menu-button");
      let importButton = menuButton.shadowRoot.querySelector(
        ".menuitem-import-file"
      );
      // Force the menu item to be visible for the test.
      importButton.hidden = false;
      return importButton;
    }

    BrowserTestUtils.synthesizeMouseAtCenter(getImportMenuItem, {}, browser);

    async function waitForFilePicker() {
      let filePickerPromise = CsvImportHelper.waitForOpenFilePicker(csvFile);
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
      await filePickerPromise;
      Assert.ok(true, "Import file picker opened");
    }

    await waitForFilePicker();
  }

  /**
   * An utility method to fetch the data from the CSV import success dialog.
   *
   * @param {browser} browser
   *        The browser object.
   * @returns {Promise<Object>} A promise that contains added, modified, noChange and errors count.
   */
  static async getCsvImportSuccessDialogData(browser) {
    return SpecialPowers.spawn(browser, [], async () => {
      let dialog = Cu.waiveXrays(
        content.document.querySelector("import-summary-dialog")
      );
      await ContentTaskUtils.waitForCondition(
        () => !dialog.hidden,
        "Waiting for the dialog to be visible"
      );

      let added = dialog.shadowRoot.querySelector(
        ".import-items-added .result-count"
      ).textContent;
      let modified = dialog.shadowRoot.querySelector(
        ".import-items-modified .result-count"
      ).textContent;
      let noChange = dialog.shadowRoot.querySelector(
        ".import-items-no-change .result-count"
      ).textContent;
      let errors = dialog.shadowRoot.querySelector(
        ".import-items-errors .result-count"
      ).textContent;
      const dialogData = {
        added,
        modified,
        noChange,
        errors,
      };
      if (dialog.shadowRoot.activeElement) {
        dialogData.l10nFocused =
          dialog.shadowRoot.activeElement.getAttribute("data-l10n-id");
      }
      return dialogData;
    });
  }

  /**
   * An utility method to fetch the data from the CSV import error dialog.
   *
   * @param {browser} browser
   *        The browser object.
   * @returns {Promise<Object>} A promise that contains the hidden state and l10n id for title, description and focused element.
   */
  static async getCsvImportErrorDialogData(browser) {
    return SpecialPowers.spawn(browser, [], async () => {
      const dialog = Cu.waiveXrays(
        content.document.querySelector("import-error-dialog")
      );
      const l10nTitle = dialog._genericDialog
        .querySelector(".error-title")
        .getAttribute("data-l10n-id");
      const l10nDescription = dialog._genericDialog
        .querySelector(".error-description")
        .getAttribute("data-l10n-id");
      return {
        hidden: dialog.hidden,
        l10nFocused:
          dialog.shadowRoot.activeElement.getAttribute("data-l10n-id"),
        l10nTitle,
        l10nDescription,
      };
    });
  }

  /**
   * An utility method to wait until CSV import is complete.
   *
   * @returns {Promise} A promise that gets resolved when the import is complete.
   */
  static async waitForImportToComplete() {
    info("Waiting for the import to complete");
    await LoginTestUtils.telemetry.waitForEventCount(1, "parent");
    TelemetryTestUtils.assertEvents(
      [["pwmgr", "mgmt_menu_item_used", "import_csv_complete"]],
      { category: "pwmgr", method: "mgmt_menu_item_used" },
      { process: "parent" }
    );
  }

  /**
   * An utility method open the about:loginsimportreport page.
   *
   * @param {browser} browser
   *        The browser object.
   * @returns {Promise<Object>} A promise that contains the about:loginsimportreport tab.
   */
  static async clickDetailedReport(browser) {
    let loadedReportTab = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:loginsimportreport",
      true
    );
    await SpecialPowers.spawn(browser, [], async () => {
      let dialog = Cu.waiveXrays(
        content.document.querySelector("import-summary-dialog")
      );
      await ContentTaskUtils.waitForCondition(
        () => !dialog.hidden,
        "Waiting for the dialog to be visible"
      );
      let detailedReportLink = dialog.shadowRoot.querySelector(
        ".open-detailed-report"
      );

      detailedReportLink.click();
    });
    return loadedReportTab;
  }

  /**
   * An utility method to fetch data from the about:loginsimportreport page.
   *
   * @param {browser} browser
   *        The browser object.
   * @returns {Promise<Object>} A promise that contains the detailed report data like added, modified, noChange, errors and rows.
   */
  static async getDetailedReportData(browser) {
    const data = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      async () => {
        function getCount(selector) {
          const attribute = content.document
            .querySelector(selector)
            .getAttribute("data-l10n-args");
          return JSON.parse(attribute).count;
        }
        const rows = [];
        for (let element of content.document.querySelectorAll(".row-details")) {
          rows.push(element.getAttribute("data-l10n-id"));
        }
        const added = getCount(".new-logins");
        const modified = getCount(".exiting-logins");
        const noChange = getCount(".duplicate-logins");
        const errors = getCount(".errors-logins");
        return {
          rows,
          added,
          modified,
          noChange,
          errors,
        };
      }
    );
    return data;
  }
}

const random = Math.round(Math.random() * 100000001);

add_setup(async function () {
  registerCleanupFunction(() => {
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_open_import_one_item_from_csv() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async browser => {
      await CsvImportHelper.clickImportFromCsvMenu(browser, [
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        `https://example.com,joe${random}@example.com,qwerty,My realm,,{${random}-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802`,
      ]);
      await CsvImportHelper.waitForImportToComplete();

      let summary = await CsvImportHelper.getCsvImportSuccessDialogData(
        browser
      );
      Assert.equal(summary.added, "1", "It should have one item as added");
      Assert.equal(
        summary.l10nFocused,
        "about-logins-import-dialog-done",
        "dismiss button should be focused"
      );
    }
  );
});

add_task(async function test_open_import_all_four_categories() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async browser => {
      const initialCsvData = [
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        `https://example1.com,existing${random},existing,,,{${random}-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924000`,
        `https://example1.com,duplicate,duplicate,,,{dddd0080-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363`,
      ];
      const updatedCsvData = [
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        `https://example1.com,added${random},added,,,,,,`,
        `https://example1.com,existing${random},modified,,,{${random}-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363`,
        `https://example1.com,duplicate,duplicate,,,{dddd0080-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363`,
        `https://example1.com,error,,,,,,,`,
      ];

      await CsvImportHelper.clickImportFromCsvMenu(browser, initialCsvData);
      await CsvImportHelper.waitForImportToComplete();
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "dismiss-button",
        {},
        browser
      );
      await CsvImportHelper.clickImportFromCsvMenu(browser, updatedCsvData);
      await CsvImportHelper.waitForImportToComplete();

      let summary = await CsvImportHelper.getCsvImportSuccessDialogData(
        browser
      );
      Assert.equal(summary.added, "1", "It should have one item as added");
      Assert.equal(
        summary.modified,
        "1",
        "It should have one item as modified"
      );
      Assert.equal(
        summary.noChange,
        "1",
        "It should have one item as unchanged"
      );
      Assert.equal(summary.errors, "1", "It should have one item as error");
    }
  );
});

add_task(async function test_open_import_all_four_detailed_report() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async browser => {
      const initialCsvData = [
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        `https://example2.com,existing${random},existing,,,{${random}-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924000`,
        "https://example2.com,duplicate,duplicate,,,{dddd0080-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363",
      ];
      const updatedCsvData = [
        "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
        `https://example2.com,added${random},added,,,,,,`,
        `https://example2.com,existing${random},modified,,,{${random}-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363`,
        "https://example2.com,duplicate,duplicate,,,{dddd0080-07a1-4bcf-86f0-7d56b9c1f48f},1582229924361,1582495972623,1582229924363",
        "https://example2.com,error,,,,,,,",
      ];

      await CsvImportHelper.clickImportFromCsvMenu(browser, initialCsvData);
      await CsvImportHelper.waitForImportToComplete();
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "dismiss-button",
        {},
        browser
      );
      await CsvImportHelper.clickImportFromCsvMenu(browser, updatedCsvData);
      await CsvImportHelper.waitForImportToComplete();
      const reportTab = await CsvImportHelper.clickDetailedReport(browser);
      const report = await CsvImportHelper.getDetailedReportData(browser);
      BrowserTestUtils.removeTab(reportTab);
      const { added, modified, noChange, errors, rows } = report;
      Assert.equal(added, 1, "It should have one item as added");
      Assert.equal(modified, 1, "It should have one item as modified");
      Assert.equal(noChange, 1, "It should have one item as unchanged");
      Assert.equal(errors, 1, "It should have one item as error");
      Assert.deepEqual(
        [
          "about-logins-import-report-row-description-added2",
          "about-logins-import-report-row-description-modified2",
          "about-logins-import-report-row-description-no-change2",
          "about-logins-import-report-row-description-error-missing-field",
        ],
        rows,
        "It should have expected rows in order"
      );
    }
  );
});

add_task(async function test_open_import_from_csv_with_invalid_file() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async browser => {
      await CsvImportHelper.clickImportFromCsvMenu(browser, [
        "invalid csv file",
      ]);

      info("Waiting for the import error dialog");
      const errorDialog = await CsvImportHelper.getCsvImportErrorDialogData(
        browser
      );
      Assert.equal(errorDialog.hidden, false, "Dialog should not be hidden");
      Assert.equal(
        errorDialog.l10nTitle,
        "about-logins-import-dialog-error-file-format-title",
        "Dialog error title should be correct"
      );
      Assert.equal(
        errorDialog.l10nDescription,
        "about-logins-import-dialog-error-file-format-description",
        "Dialog error description should be correct"
      );
      Assert.equal(
        errorDialog.l10nFocused,
        "about-logins-import-dialog-error-learn-more",
        "Learn more link should be focused."
      );
    }
  );
});
