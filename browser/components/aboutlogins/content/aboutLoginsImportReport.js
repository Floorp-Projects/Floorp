/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Begin code that executes on page load.

document.dispatchEvent(
  new CustomEvent("AboutLoginsImportReportInit", { bubbles: true })
);

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "ImportReportData":
      // TODO: Bug 1649940 - Render the report summary
      console.log(
        "about:loginsimportreport: Got ImportReportData message: ",
        event
      );
      break;
  }
});
