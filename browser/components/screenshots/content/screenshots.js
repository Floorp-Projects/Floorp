/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ScreenshotsUI extends HTMLElement {
  constructor() {
    super();
  }
  connectedCallback() {
    this.initialize();
  }

  initialize() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    let template = this.ownerDocument.getElementById(
      "screenshots-dialog-template"
    );
    let templateContent = template.content;
    this.appendChild(templateContent.cloneNode(true));

    this._cancelButton = this.querySelector(".highlight-button-cancel");
    this._cancelButton.addEventListener("click", this);
    this._copyButton = this.querySelector(".highlight-button-copy");
    this._copyButton.addEventListener("click", this);
    this._downloadButton = this.querySelector(".highlight-button-download");
    this._downloadButton.addEventListener("click", this);
  }

  handleEvent(event) {
    if (event.type == "click" && event.target == this._cancelButton) {
      //TODO hook up close functionality
    } else if (event.type == "click" && event.target == this._copyButton) {
      //TODO hook up copy functionality
    } else if (event.type == "click" && event.target == this._downloadButton) {
      //TODO hook up download functionality
    }
  }
}
customElements.define("screenshots-ui", ScreenshotsUI);
