/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

/**
 * HTML5 file saver to provide a standard download interface with a "Save As"
 * dialog
 *
 * @param {object} blob - A blob object will be downloaded
 * @param {string} filename - Given a file name which will display in "Save As" dialog
 * @param {object} document - Optional. A HTML document for creating a temporary anchor
 *                            for triggering a file download.
 */
function saveAs(blob, filename = "", doc = document) {
  const url = URL.createObjectURL(blob);
  const a = doc.createElement("a");
  doc.body.appendChild(a);
  a.style = "display: none";
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
  a.remove();
}

exports.saveAs = saveAs;
