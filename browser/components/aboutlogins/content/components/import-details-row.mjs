/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const resultToUiData = {
  no_change: {
    message: "about-logins-import-report-row-description-no-change2",
  },
  modified: {
    message: "about-logins-import-report-row-description-modified2",
  },
  added: {
    message: "about-logins-import-report-row-description-added2",
  },
  error: {
    message: "about-logins-import-report-row-description-error",
    isError: true,
  },
  error_multiple_values: {
    message: "about-logins-import-report-row-description-error-multiple-values",
    isError: true,
  },
  error_missing_field: {
    message: "about-logins-import-report-row-description-error-missing-field",
    isError: true,
  },
};

export default class ImportDetailsRow extends HTMLElement {
  constructor(number, reportRow) {
    super();
    this._login = reportRow;

    let rowElement = document
      .querySelector("#import-details-row-template")
      .content.cloneNode(true);

    const uiData = resultToUiData[reportRow.result];
    if (uiData.isError) {
      this.classList.add("error");
    }
    const rowCount = rowElement.querySelector(".row-count");
    const rowDetails = rowElement.querySelector(".row-details");
    while (rowElement.childNodes.length) {
      this.appendChild(rowElement.childNodes[0]);
    }
    document.l10n.connectRoot(this);
    document.l10n.setAttributes(
      rowCount,
      "about-logins-import-report-row-index",
      {
        number,
      }
    );
    document.l10n.setAttributes(rowDetails, uiData.message, {
      field: reportRow.field_name,
    });
  }
}
customElements.define("import-details-row", ImportDetailsRow);
