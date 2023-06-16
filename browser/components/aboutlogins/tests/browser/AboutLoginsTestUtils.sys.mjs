/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An utility class to help out with the about:logins and about:loginsimportreport DOM interaction for the tests.
 *
 */
export class AboutLoginsTestUtils {
  /**
   * An utility method to fetch the data from the CSV import success dialog.
   *
   * @param {content} content
   *        The content object.
   * @param {ContentTaskUtils} ContentTaskUtils
   *        The ContentTaskUtils object.
   * @returns {Promise<Object>} A promise that contains added, modified, noChange and errors count.
   */
  static async getCsvImportSuccessDialogData(content, ContentTaskUtils) {
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
    return {
      added,
      modified,
      noChange,
      errors,
      l10nFocused: dialog.shadowRoot.activeElement.getAttribute("data-l10n-id"),
    };
  }

  /**
   * An utility method to fetch the data from the CSV import error dialog.
   *
   * @param {content} content
   *        The content object.
   * @returns {Promise<Object>} A promise that contains the hidden state and l10n id for title, description and focused element.
   */
  static async getCsvImportErrorDialogData(content) {
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
      l10nFocused: dialog.shadowRoot.activeElement.getAttribute("data-l10n-id"),
      l10nTitle,
      l10nDescription,
    };
  }

  /**
   * An utility method to fetch data from the about:loginsimportreport page.
   * It also cleans up the tab so you don't have to.
   *
   * @param {content} content
   *        The content object.
   * @returns {Promise<Object>} A promise that contains the detailed report data like added, modified, noChange, errors and rows.
   */
  static async getCsvImportReportData(content) {
    const rows = [];
    for (let element of content.document.querySelectorAll(".row-details")) {
      rows.push(element.getAttribute("data-l10n-id"));
    }
    const added = content.document.querySelector(
      ".new-logins .result-count"
    ).textContent;
    const modified = content.document.querySelector(
      ".exiting-logins .result-count"
    ).textContent;
    const noChange = content.document.querySelector(
      ".duplicate-logins .result-count"
    ).textContent;
    const errors = content.document.querySelector(
      ".errors-logins .result-count"
    ).textContent;
    return {
      rows,
      added,
      modified,
      noChange,
      errors,
    };
  }
}
