/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gMoreFromMozillaPane = {
  initialized: false,

  openURL(url) {
    const URL_PARAMS = {
      utm_source: "about-preferences",
      utm_campaign: "morefrommozilla-na",
      utm_medium: "firefox-release-browser",
      entrypoint_experiment: "morefrommozilla",
      entrypoint_variation: "a",
    };

    let pageUrl = new URL(url);
    for (let [key, val] of Object.entries(URL_PARAMS)) {
      pageUrl.searchParams.append(key, val);
    }

    let mainWindow = window.windowRoot.ownerGlobal;
    mainWindow.openTrustedLinkIn(pageUrl.toString(), "tab");
  },

  async init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;
    document
      .getElementById("moreFromMozillaCategory")
      .removeAttribute("data-hidden-from-search");

    document.getElementById("mozillaVPN").addEventListener("click", function() {
      gMoreFromMozillaPane.openURL("https://www.mozilla.org/products/vpn/");
    });

    document.getElementById("fxMobile").addEventListener("click", function() {
      gMoreFromMozillaPane.openURL(
        "https://www.mozilla.org/en-US/firefox/browsers/mobile/"
      );
    });

    document
      .getElementById("mozillaRally")
      .addEventListener("click", function() {
        gMoreFromMozillaPane.openURL("https://rally.mozilla.org/");
      });
  },
};
