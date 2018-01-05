/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Set dom.webcomponents.enabled pref to true and loads an iframe, to ensure
 * that the Element instance is loaded with the correct value of the
 * preference.
 *
 * @return {Promise} promise that resolves when iframe is loaded.
 */
function setWebComponentsPrefAndCreateIframe(aSrcDoc) {
  return new Promise(function (aResolve, aReject) {
    SpecialPowers.pushPrefEnv({
      set: [
        ["dom.webcomponents.enabled", true]
      ]
    }, () => {
      let iframe = document.createElement("iframe");
      iframe.onload = function () { aResolve(iframe.contentDocument); }
      iframe.onerror = function () { aReject('Failed to load iframe'); }
      if (aSrcDoc) {
        iframe.srcdoc = aSrcDoc;
      }
      document.body.appendChild(iframe);
    });
  });
}
