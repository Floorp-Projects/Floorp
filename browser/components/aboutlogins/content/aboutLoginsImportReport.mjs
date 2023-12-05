/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ImportDetailsRow from "./components/import-details-row.mjs";

const detailsLoginsList = document.querySelector(".logins-list");
const detailedNewCount = document.querySelector(".new-logins");
const detailedExitingCount = document.querySelector(".exiting-logins");
const detailedDuplicateCount = document.querySelector(".duplicate-logins");
const detailedErrorsCount = document.querySelector(".errors-logins");

document.dispatchEvent(
  new CustomEvent("AboutLoginsImportReportInit", { bubbles: true })
);

function importReportDataHandler(event) {
  switch (event.detail.messageType) {
    case "ImportReportData":
      const logins = event.detail.value;
      const report = {
        added: 0,
        modified: 0,
        no_change: 0,
        error: 0,
      };
      for (let loginRow of logins) {
        if (loginRow.result.includes("error")) {
          report.error++;
        } else {
          report[loginRow.result]++;
        }
      }
      document.l10n.setAttributes(
        detailedNewCount,
        "about-logins-import-report-added2",
        { count: report.added }
      );
      document.l10n.setAttributes(
        detailedExitingCount,
        "about-logins-import-report-modified2",
        { count: report.modified }
      );
      document.l10n.setAttributes(
        detailedDuplicateCount,
        "about-logins-import-report-no-change2",
        { count: report.no_change }
      );
      document.l10n.setAttributes(
        detailedErrorsCount,
        "about-logins-import-report-error",
        { count: report.error }
      );
      if (report.no_change > 0) {
        detailedDuplicateCount
          .querySelector(".not-imported")
          .classList.toggle("not-imported-hidden");
      }
      if (report.error > 0) {
        detailedErrorsCount
          .querySelector(".not-imported")
          .classList.toggle("not-imported-hidden");
      }

      detailsLoginsList.innerHTML = "";
      let fragment = document.createDocumentFragment();
      for (let index = 0; index < logins.length; index++) {
        const row = new ImportDetailsRow(index + 1, logins[index]);
        fragment.appendChild(row);
      }
      detailsLoginsList.appendChild(fragment);
      window.removeEventListener(
        "AboutLoginsChromeToContent",
        importReportDataHandler
      );
      document.dispatchEvent(
        new CustomEvent("AboutLoginsImportReportReady", { bubbles: true })
      );
      break;
  }
}

window.addEventListener("AboutLoginsChromeToContent", importReportDataHandler);
