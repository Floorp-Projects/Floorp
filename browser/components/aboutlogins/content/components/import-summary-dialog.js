/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { initDialog } from "../aboutLoginsUtils.js";

export default class ImportSummaryDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    initDialog(this, "#import-summary-dialog-template");
    this._added = this.shadowRoot.querySelector(".import-items-added");
    this._modified = this.shadowRoot.querySelector(".import-items-modified");
    this._noChange = this.shadowRoot.querySelector(".import-items-no-change");
    this._error = this.shadowRoot.querySelector(".import-items-errors");
    this._genericDialog = this.shadowRoot.querySelector("generic-dialog");
  }

  show({ logins }) {
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
    this._updateCount(
      report.added,
      this._added,
      "about-logins-import-dialog-items-added"
    );
    this._updateCount(
      report.modified,
      this._modified,
      "about-logins-import-dialog-items-modified"
    );
    this._updateCount(
      report.no_change,
      this._noChange,
      "about-logins-import-dialog-items-no-change"
    );
    this._updateCount(
      report.error,
      this._error,
      "about-logins-import-dialog-items-error"
    );
    this._noChange.querySelector(".result-meta").hidden =
      report.no_change === 0;
    this._error.querySelector(".result-meta").hidden = report.error === 0;
    this._genericDialog.show();
    window.AboutLoginsUtils.setFocus(this._genericDialog._dismissButton);
  }

  _updateCount(count, component, message) {
    if (count != document.l10n.getAttributes(component).args.count) {
      document.l10n.setAttributes(component, message, { count });
    }
  }
}
customElements.define("import-summary-dialog", ImportSummaryDialog);
