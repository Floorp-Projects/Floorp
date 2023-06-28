/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

class OverviewInView extends ViewPage {
  constructor() {
    super();
    this.pageType = "overview";
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {}

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <div class="sticky-container bottom-fade">
        <h2 class="page-header" data-l10n-id="firefoxview-overview-header"></h2>
      </div>
      <div class="cards-container">
        <slot></slot>
      </div>
    `;
  }
}
customElements.define("view-overview", OverviewInView);
