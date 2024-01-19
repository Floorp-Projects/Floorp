/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  ChromeUtils.defineESModuleGetters(this, {
    ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
  });

  class ScreenshotsButtons extends MozXULElement {
    static get markup() {
      return `
      <html:link rel="stylesheet" href="chrome://global/skin/global.css"/>
      <html:link rel="stylesheet" href="chrome://browser/content/screenshots/screenshots-buttons.css"/>
      <html:button class="visible-page footer-button" data-l10n-id="screenshots-save-visible-button"></html:button>
      <html:button class="full-page footer-button" data-l10n-id="screenshots-save-page-button"></html:button>
      `;
    }

    connectedCallback() {
      const shadowRoot = this.attachShadow({ mode: "open" });
      document.l10n.connectRoot(shadowRoot);

      let fragment = MozXULElement.parseXULToFragment(this.constructor.markup);
      this.shadowRoot.append(fragment);

      let visibleButton = shadowRoot.querySelector(".visible-page");
      visibleButton.onclick = function () {
        ScreenshotsUtils.doScreenshot(gBrowser.selectedBrowser, "visible");
      };

      let fullpageButton = shadowRoot.querySelector(".full-page");
      fullpageButton.onclick = function () {
        ScreenshotsUtils.doScreenshot(gBrowser.selectedBrowser, "full_page");
      };
    }

    disconnectedCallback() {
      document.l10n.disconnectRoot(this.shadowRoot);
    }

    /**
     * Focus the last used button.
     * This will default to the visible page button.
     * @param {String} buttonToFocus
     */
    focusButton(buttonToFocus) {
      if (buttonToFocus === "fullpage") {
        this.shadowRoot
          .querySelector(".full-page")
          .focus({ focusVisible: true });
      } else {
        this.shadowRoot
          .querySelector(".visible-page")
          .focus({ focusVisible: true });
      }
    }
  }

  customElements.define("screenshots-buttons", ScreenshotsButtons, {
    extends: "toolbar",
  });
}
