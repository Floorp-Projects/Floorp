/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

class OpenTabsInView extends ViewPage {
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
      itemTemplates.push(html` <p>Open Tab Row ${i}</p> `);
    }

    return html`
      <ul>
        ${itemTemplates}
      </ul>
    `;
  }
}
customElements.define("view-opentabs", OpenTabsInView);
