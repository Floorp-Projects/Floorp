/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class ViewPage extends MozLitElement {
  static get properties() {
    return {
      selectedTab: { type: Boolean },
      overview: { type: Boolean },
    };
  }

  constructor() {
    super();
    this.selectedTab = false;
    this.overview = Boolean(this.closest("VIEW-OVERVIEW"));
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {}

  enter() {
    this.selectedTab = true;
  }

  exit() {
    this.selectedTab = false;
  }
}
