/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class ScreenshotsButtons extends MozXULElement {
    static get markup() {
      return `
      <html:link rel="stylesheet" href="chrome://global/skin/global.css"/>
      <html:link rel="stylesheet" href="chrome://browser/content/screenshots/screenshots-buttons.css"/>
      <html:button class="visible-page panel-footer-button" data-l10n-id="screenshots-save-visible-button"></html:button>
      <html:button class="full-page panel-footer-button" data-l10n-id="screenshots-save-page-button"></html:button>
      `;
    }

    connectedCallback() {
      const shadowRoot = this.attachShadow({ mode: "open" });
      document.l10n.connectRoot(shadowRoot);

      let fragment = MozXULElement.parseXULToFragment(this.constructor.markup);
      this.shadowRoot.append(fragment);

      let button1 = shadowRoot.querySelector(".visible-page");
      button1.onclick = function () {
        Services.obs.notifyObservers(
          gBrowser.ownerGlobal,
          "screenshots-take-screenshot",
          "visible"
        );
      };

      let button2 = shadowRoot.querySelector(".full-page");
      button2.onclick = function () {
        Services.obs.notifyObservers(
          gBrowser.ownerGlobal,
          "screenshots-take-screenshot",
          "full-page"
        );
      };
    }

    disconnectedCallback() {
      document.l10n.disconnectRoot(this.shadowRoot);
    }

    focusFirst(focusOptions) {
      this.shadowRoot.querySelector("button:enabled").focus(focusOptions);
    }
  }
  customElements.define("screenshots-buttons", ScreenshotsButtons, {
    extends: "toolbar",
  });
}
