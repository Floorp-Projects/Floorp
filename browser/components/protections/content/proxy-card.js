/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const PROXY_EXTENSION_URL = RPMGetStringPref(
  "browser.contentblocking.report.proxy_extension.url",
  ""
);

export default class ProxyCard {
  constructor(document) {
    this.doc = document;
  }

  init() {
    const proxyExtensionLink = this.doc.getElementById(
      "get-proxy-extension-link"
    );
    proxyExtensionLink.href = PROXY_EXTENSION_URL;

    // Show the Proxy card
    RPMSendQuery("GetShowProxyCard", {}).then(shouldShow => {
      const proxyCard = this.doc.querySelector(".proxy-card");
      proxyCard.classList.toggle("hidden", !shouldShow);
    });
  }
}
