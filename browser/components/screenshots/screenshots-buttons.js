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
    static #template = null;

    static get markup() {
      return `
        <html:link rel="stylesheet" href="chrome://global/skin/global.css" />
        <html:link rel="stylesheet" href="chrome://browser/content/screenshots/screenshots-buttons.css" />
        <html:moz-button-group>
          <html:button id="visible-page" class="screenshot-button footer-button" data-l10n-id="screenshots-save-visible-button"></html:button>
          <html:button id="full-page" class="screenshot-button footer-button primary" data-l10n-id="screenshots-save-page-button"></html:button>
        </html:moz-button-group>

      `;
    }

    static get fragment() {
      if (!ScreenshotsButtons.#template) {
        ScreenshotsButtons.#template = MozXULElement.parseXULToFragment(
          ScreenshotsButtons.markup
        );
      }
      return ScreenshotsButtons.#template;
    }

    connectedCallback() {
      const shadowRoot = this.attachShadow({ mode: "open" });
      document.l10n.connectRoot(shadowRoot);

      this.shadowRoot.append(ScreenshotsButtons.fragment);

      let visibleButton = shadowRoot.getElementById("visible-page");
      visibleButton.onclick = function () {
        ScreenshotsUtils.doScreenshot(gBrowser.selectedBrowser, "visible");
      };

      let fullpageButton = shadowRoot.getElementById("full-page");
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
    async focusButton(buttonToFocus) {
      await this.shadowRoot.querySelector("moz-button-group").updateComplete;
      if (buttonToFocus === "fullpage") {
        this.shadowRoot
          .getElementById("full-page")
          .focus({ focusVisible: true });
      } else if (buttonToFocus === "first") {
        this.shadowRoot
          .querySelector("moz-button-group")
          .firstElementChild.focus({ focusVisible: true });
      } else if (buttonToFocus === "last") {
        this.shadowRoot
          .querySelector("moz-button-group")
          .lastElementChild.focus({ focusVisible: true });
      } else {
        this.shadowRoot
          .getElementById("visible-page")
          .focus({ focusVisible: true });
      }
    }
  }

  customElements.define("screenshots-buttons", ScreenshotsButtons, {
    extends: "toolbar",
  });
}
