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
      <html:link rel="stylesheet" href="chrome://browser/content/screenshots/screenshots-buttons.css"/>
      <html:div id="screenshots-buttons" class="all-buttons-container">
        <html:button class="visible-page" data-l10n-id="screenshots-save-visible-button"></html:button>
        <html:button class="full-page" data-l10n-id="screenshots-save-page-button"></html:button>
      </html:div>
      `;
    }

    connectedCallback() {
      const shadowRoot = this.attachShadow({ mode: "open" });
      document.l10n.connectRoot(shadowRoot);

      let fragment = MozXULElement.parseXULToFragment(this.constructor.markup);
      this.shadowRoot.append(fragment);

      let button1 = shadowRoot.querySelector(".visible-page");
      button1.onclick = function() {
        Services.obs.notifyObservers(
          gBrowser.ownerGlobal,
          "screenshots-take-screenshot",
          "visible"
        );
      };

      let button2 = shadowRoot.querySelector(".full-page");
      button2.onclick = function() {
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
  }
  customElements.define("screenshots-buttons", ScreenshotsButtons, {
    extends: "toolbar",
  });
}
