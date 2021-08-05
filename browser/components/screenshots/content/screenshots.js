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
  }

  handleEvent(event) {
    // TODO: handle button clicks
  }
}
customElements.define("screenshots-ui", ScreenshotsUI);
