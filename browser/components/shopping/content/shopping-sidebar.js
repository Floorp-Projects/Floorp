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

    static get observedAttributes() {
      return ["url"];
    }

    static get markup() {
      return `
        <browser
          class="shopping-sidebar"
          autoscroll="false"
          disablefullscreen="true"
          disablehistory="true"
          flex="1"
          message="true"
          remotetype="privilegedabout"
          remote="true"
          selectmenulist="contentselectdropdown"
          src="chrome://browser/content/shopping/shopping.html"
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

    attributeChangedCallback(name, oldValue, newValue) {
      // If newValue is truthy, we have a product URL; lazily init the sidebar.
      if (newValue) {
        this.initialize();
      }

      this.update(newValue);
    }

    // TODO (bug 1840847): Use messaging instead of setting `src` repeatedly.
    update(productURL) {
      if (!productURL) {
        this.#browser.src = "chrome://browser/content/shopping/shopping.html";
        return;
      }
      this.#browser.src =
        "chrome://browser/content/shopping/shopping.html?url=" + productURL;
    }
  }

  customElements.define("shopping-sidebar", ShoppingSidebar);
}
