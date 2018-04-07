/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Opens |url| in a new tab.
 */
exports.openLink = async function(url, toolbox) {
  const parentDoc = toolbox.doc;
  if (!parentDoc) {
    return;
  }

  const win = parentDoc.querySelector("window");
  if (!win) {
    return;
  }

  const top = win.ownerDocument.defaultView.top;
  if (!top || typeof top.openUILinkIn !== "function") {
    return;
  }

  top.openUILinkIn(url, "tab");
};
