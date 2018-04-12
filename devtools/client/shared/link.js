/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Retrieve the top window for the provided toolbox that should have the expected
 * open*LinkIn methods.
 */
function _getTopWindow(toolbox) {
  const parentDoc = toolbox.doc;
  if (!parentDoc) {
    return null;
  }

  const win = parentDoc.querySelector("window");
  if (!win) {
    return null;
  }

  return win.ownerDocument.defaultView.top;
}

/**
 * Opens a |url| controlled by webcontent in a new tab.
 *
 * @param {String} url
 *        The url to open.
 * @param {Toolbox} toolbox
 *        Toolbox reference to find the parent window.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openWebLink = async function(url, toolbox, options) {
  const top = _getTopWindow(toolbox);
  if (top && typeof top.openWebLinkIn === "function") {
    top.openWebLinkIn(url, "tab", options);
  }
};

/**
 * Open a trusted |url| in a new tab using the SystemPrincipal.
 *
 * @param {String} url
 *        The url to open.
 * @param {Toolbox} toolbox
 *        Toolbox reference to find the parent window.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openTrustedLink = async function(url, toolbox, options) {
  const top = _getTopWindow(toolbox);
  if (top && typeof top.openTrustedLinkIn === "function") {
    top.openTrustedLinkIn(url, "tab", options);
  }
};
