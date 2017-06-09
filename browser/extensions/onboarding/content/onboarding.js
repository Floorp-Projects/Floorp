/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global content */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

const ONBOARDING_CSS_URL = "resource://onboarding/onboarding.css";
const ABOUT_HOME_URL = "about:home";
const ABOUT_NEWTAB_URL = "about:newtab";
const BUNDLE_URI = "chrome://onboarding/locale/onboarding.properties";

/**
 * The script won't be initialized if we turned off onboarding by
 * setting "browser.onboarding.enabled" to false.
 */
class Onboarding {
  constructor(contentWindow) {
    this.init(contentWindow);
    this._bundle = Services.strings.createBundle(BUNDLE_URI);
  }

  async init(contentWindow) {
    this._window = contentWindow;
    // We want to create and append elements after CSS is loaded so
    // no flash of style changes and no additional reflow.
    await this._loadCSS();
    this._overlayIcon = this._renderOverlayIcon();
    this._overlay = this._renderOverlay();
    this._window.document.body.appendChild(this._overlayIcon);
    this._window.document.body.appendChild(this._overlay);

    this._overlayIcon.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    // Destroy on unload. This is to ensure we remove all the stuff we left.
    // No any leak out there.
    this._window.addEventListener("unload", () => this.destroy());
  }

  handleEvent(evt) {
    switch (evt.target.id) {
      case "onboarding-overlay-icon":
      case "onboarding-overlay-close-btn":
      // If the clicking target is directly on the outer-most overlay,
      // that means clicking outside the tour content area.
      // Let's toggle the overlay.
      case "onboarding-overlay":
        this.toggleOverlay();
        break;
    }
  }

  destroy() {
    this._overlayIcon.remove();
    this._overlay.remove();
  }

  toggleOverlay() {
    this._overlay.classList.toggle("opened");
  }

  _renderOverlay() {
    const BRAND_SHORT_NAME = Services.strings
                         .createBundle("chrome://branding/locale/brand.properties")
                         .GetStringFromName("brandShortName");
    let div = this._window.document.createElement("div");
    div.id = "onboarding-overlay";
    // Here we use `innerHTML` is for more friendly reading.
    // The security should be fine because this is not from an external input.
    // We're not shipping yet so l10n strings is going to be closed for now.
    div.innerHTML = `
      <div id="onboarding-overlay-dialog">
        <span id="onboarding-overlay-close-btn"></span>
        <header id="onboarding-header"></header>
        <nav>
          <ul></ul>
        </nav>
        <footer>
        </footer>
      </div>
    `;

    div.querySelector("#onboarding-header").textContent =
       this._bundle.formatStringFromName("onboarding.overlay-title", [BRAND_SHORT_NAME], 1);
    return div;
  }

  _renderOverlayIcon() {
    let img = this._window.document.createElement("div");
    img.id = "onboarding-overlay-icon";
    return img;
  }

  _loadCSS() {
    // Returning a Promise so we can inform caller of loading complete
    // by resolving it.
    return new Promise(resolve => {
      let doc = this._window.document;
      let link = doc.createElement("link");
      link.rel = "stylesheet";
      link.type = "text/css";
      link.href = ONBOARDING_CSS_URL;
      link.addEventListener("load", resolve);
      doc.head.appendChild(link);
    });
  }
}

addEventListener("load", function onLoad(evt) {
  if (!content || evt.target != content.document) {
    return;
  }
  removeEventListener("load", onLoad);

  let window = evt.target.defaultView;
  // Load onboarding module only when we enable it.
  if ((window.location.href == ABOUT_NEWTAB_URL ||
       window.location.href == ABOUT_HOME_URL) &&
      Services.prefs.getBoolPref("browser.onboarding.enabled", false)) {

    window.requestIdleCallback(() => {
      new Onboarding(window);
    });
  }
}, true);
