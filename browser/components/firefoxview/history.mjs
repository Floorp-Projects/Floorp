/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/card-container.mjs";

class HistoryInView extends ViewPage {
  constructor() {
    super();
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {}

  render() {
    if (!this.selectedTab && !this.overview) {
      return null;
    }
    let numRows = this.overview ? 5 : 10;
    const itemTemplates = [];

    for (let i = 1; i <= numRows; i++) {
      itemTemplates.push(html` <p>History Row ${i}</p> `);
    }

    return html`
      <card-container
        .viewAllPage=${this.overview ? "history" : null}
        ?preserveCollapseState=${this.overview ? true : null}
      >
        <h2 slot="header" data-l10n-id="firefoxview-history-header"></h2>
        <ul slot="main">
          ${itemTemplates}
        </ul>
      </card-container>
    `;
  }
}
customElements.define("view-history", HistoryInView);
