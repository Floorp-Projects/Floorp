/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { initDialog } from "../aboutLoginsUtils.mjs";

export default class ImportErrorDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
    this._errorMessages = {};
    this._errorMessages.CONFLICTING_VALUES_ERROR = {
      title: "about-logins-import-dialog-error-conflicting-values-title",
      description:
        "about-logins-import-dialog-error-conflicting-values-description",
    };
    this._errorMessages.FILE_FORMAT_ERROR = {
      title: "about-logins-import-dialog-error-file-format-title",
      description: "about-logins-import-dialog-error-file-format-description",
    };
    this._errorMessages.FILE_PERMISSIONS_ERROR = {
      title: "about-logins-import-dialog-error-file-permission-title",
      description:
        "about-logins-import-dialog-error-file-permission-description",
    };
    this._errorMessages.UNABLE_TO_READ_ERROR = {
      title: "about-logins-import-dialog-error-unable-to-read-title",
      description:
        "about-logins-import-dialog-error-unable-to-read-description",
    };
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    const shadowRoot = initDialog(this, "#import-error-dialog-template");
    this._titleElement = shadowRoot.querySelector(".error-title");
    this._descriptionElement = shadowRoot.querySelector(".error-description");
    this._genericDialog = this.shadowRoot.querySelector("generic-dialog");
    this._focusedElement = this.shadowRoot.querySelector("a");
    const tryImportAgain = this.shadowRoot.querySelector(".try-import-again");
    tryImportAgain.addEventListener("click", () => {
      this._genericDialog.hide();
      document.dispatchEvent(
        new CustomEvent("AboutLoginsImportFromFile", { bubbles: true })
      );
    });
  }

  show(errorType) {
    const { title, description } = this._errorMessages[errorType];
    document.l10n.setAttributes(this._titleElement, title);
    document.l10n.setAttributes(this._descriptionElement, description);
    this._genericDialog.show();
    window.AboutLoginsUtils.setFocus(this._focusedElement);
  }
}
customElements.define("import-error-dialog", ImportErrorDialog);
