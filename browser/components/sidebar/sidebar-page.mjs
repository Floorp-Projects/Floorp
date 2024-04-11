/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

export class SidebarPage extends MozLitElement {
  constructor() {
    super();
    this.clearDocument = this.clearDocument.bind(this);
  }

  connectedCallback() {
    super.connectedCallback();
    this.ownerGlobal.addEventListener("beforeunload", this.clearDocument);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.ownerGlobal.removeEventListener("beforeunload", this.clearDocument);
  }

  /**
   * Clear out the document so the disconnectedCallback() will trigger properly
   * and all of the custom elements can cleanup.
   */
  clearDocument() {
    this.ownerGlobal.document.body.textContent = "";
  }

  /**
   * The common stylesheet for all sidebar pages.
   *
   * @returns {TemplateResult}
   */
  stylesheet() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar.css"
      />
    `;
  }
}
