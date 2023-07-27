/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class ShoppingSidebar extends MozXULElement {
    #browser;
    #initialized;

    static get markup() {
      return `
        <browser
          class="shopping-sidebar"
          autoscroll="false"
          disablefullscreen="true"
          disablehistory="true"
          flex="1"
          message="true"
          remoteType="privilegedabout"
          maychangeremoteness="true"
          remote="true"
          src="about:shoppingsidebar"
          type="content"
          />
      `;
    }

    constructor() {
      super();
    }

    connectedCallback() {
      this.initialize();
    }

    initialize() {
      if (this.#initialized) {
        return;
      }
      this.appendChild(this.constructor.fragment);
      this.#browser = this.querySelector(".shopping-sidebar");

      this.#initialized = true;
    }
  }

  customElements.define("shopping-sidebar", ShoppingSidebar);
}
