/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Loads an iframe.
 *
 * @return {Promise} promise that resolves when iframe is loaded.
 */
function createIframe(aSrcDoc) {
  return new Promise(function (aResolve, aReject) {
    let iframe = document.createElement("iframe");
    iframe.onload = function () {
      aResolve(iframe.contentDocument);
    };
    iframe.onerror = function () {
      aReject("Failed to load iframe");
    };
    if (aSrcDoc) {
      iframe.srcdoc = aSrcDoc;
    }
    document.body.appendChild(iframe);
  });
}
