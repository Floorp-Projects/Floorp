/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

/**
 * Retrieve the most recent chrome window.
 */
function _getTopWindow() {
  return Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
}

/**
 * Opens a |url| controlled by webcontent in a new tab.
 *
 * @param {String} url
 *        The url to open.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openWebLink = async function(url, options) {
  const top = _getTopWindow();
  if (top && typeof top.openWebLinkIn === "function") {
    top.openWebLinkIn(url, "tab", options);
  }
};

/**
 * Open a trusted |url| in a new tab using the SystemPrincipal.
 *
 * @param {String} url
 *        The url to open.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openTrustedLink = async function(url, options) {
  const top = _getTopWindow();
  if (top && typeof top.openTrustedLinkIn === "function") {
    top.openTrustedLinkIn(url, "tab", options);
  }
};
