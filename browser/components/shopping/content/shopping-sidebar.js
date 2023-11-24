/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  const SHOPPING_SIDEBAR_WIDTH_PREF =
    "browser.shopping.experience2023.sidebarWidth";
  const SHOPPING_SIDEBAR_WIDTH_VAR = "--shopping-sidebar-width";
  const SHOPPINGS_SIDEBAR_WIDTH_TRANSLATE_X_VAR =
    "--shopping-sidebar-width-translate-x";
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
      this.resizeObserverFn = this.resizeObserverFn.bind(this);
      this.appendChild(this.constructor.fragment);
      this.#browser = this.querySelector(".shopping-sidebar");

      let previousWidth = Services.prefs.getIntPref(
        SHOPPING_SIDEBAR_WIDTH_PREF,
        0
      );
      if (previousWidth > 0) {
        this.style.setProperty(
          SHOPPING_SIDEBAR_WIDTH_VAR,
          `${previousWidth}px`
        );
      }

      this.resizeObserver = new ResizeObserver(this.resizeObserverFn);
      this.resizeObserver.observe(this);

      this.#initialized = true;
    }

    resizeObserverFn() {
      Services.prefs.setIntPref(SHOPPING_SIDEBAR_WIDTH_PREF, this.scrollWidth);

      /* Setting `--shopping-sidebar-width` directly would cause the resize observer to loop.
       * Update `shopping-sidebar-width-translate-x` instead to prevent this
       * after opening or closing the sidebar. */
      this.style.setProperty(
        SHOPPINGS_SIDEBAR_WIDTH_TRANSLATE_X_VAR,
        `${this.scrollWidth}px`
      );
    }
  }

  customElements.define("shopping-sidebar", ShoppingSidebar);
}
