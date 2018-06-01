/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

/*
 * Append a stylesheet to the provided XUL document.
 *
 * @param  {Document} xulDocument
 *         The XUL document where the stylesheet should be appended.
 * @param  {String} url
 *         The url of the stylesheet to load.
 * @return {Object}
 *         - styleSheet {XMLStylesheetProcessingInstruction} the instruction node created.
 *         - loadPromise {Promise} that will resolve/reject when the stylesheet finishes
 *           or fails to load.
 */
function appendStyleSheet(xulDocument, url) {
  const styleSheetAttr = `href="${url}" type="text/css"`;
  const styleSheet = xulDocument.createProcessingInstruction(
    "xml-stylesheet", styleSheetAttr);
  const loadPromise = new Promise((resolve, reject) => {
    function onload() {
      styleSheet.removeEventListener("load", onload);
      styleSheet.removeEventListener("error", onerror);
      resolve();
    }
    function onerror() {
      styleSheet.removeEventListener("load", onload);
      styleSheet.removeEventListener("error", onerror);
      reject("Failed to load theme file " + url);
    }

    styleSheet.addEventListener("load", onload);
    styleSheet.addEventListener("error", onerror);
  });
  xulDocument.insertBefore(styleSheet, xulDocument.documentElement);
  return {styleSheet, loadPromise};
}

exports.appendStyleSheet = appendStyleSheet;
