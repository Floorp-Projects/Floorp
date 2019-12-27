/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getFormatStr, getStr } = require("./l10n");

/**
 * Get a human-friendly style source path to display in the Changes panel.
 * For element inline styles, return a string indicating that.
 * For inline stylesheets, return a string indicating that plus the stylesheet's index.
 * For URLs, return just the stylesheet filename.
 *
 * @param {Object} source
 *        Information about the style source. Contains:
 *        - type: {String} One of "element" or "stylesheet"
 *        - href: {String|null} Stylesheet URL or document URL for elmeent inline styles
 *        - index: {Number} Position of the stylesheet in its document's stylesheet list.
 * @return {String}
 */
function getSourceForDisplay(source) {
  let href;

  switch (source.type) {
    case "element":
      href = getStr("changes.elementStyleLabel");
      break;
    case "inline":
      href = getFormatStr("changes.inlineStyleSheetLabel", `#${source.index}`);
      break;
    case "stylesheet":
      const url = new URL(source.href);
      href = url.pathname.substring(url.pathname.lastIndexOf("/") + 1);
      break;
  }

  return href;
}

module.exports.getSourceForDisplay = getSourceForDisplay;
