/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { css, html } from "chrome://global/content/vendor/lit.all.mjs";
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

  static get styles() {
    return css`
      div {
        border: 1px solid black;
        border-radius: 10px;
        width: 100%;
      }
  }
  `;
  }

  render() {
    return html` <slot></slot> `;
  }
}
customElements.define("view-overview", OverviewInView);
