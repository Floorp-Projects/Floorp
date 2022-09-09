/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");
const {
  TabDescriptorFactory,
} = require("devtools/client/framework/tab-descriptor-factory");

/**
 * Retrieve the most recent chrome window.
 */
function _getTopWindow() {
  // Try the main application window, such as a browser window.
  let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  if (win?.openWebLinkIn && win?.openTrustedLinkIn) {
    return win;
  }
  // For non-browser cases like Browser Toolbox, try any chrome window.
  win = Services.wm.getMostRecentWindow(null);
  if (win?.openWebLinkIn && win?.openTrustedLinkIn) {
    return win;
  }
  return null;
}

/**
 * Opens a |url| that does not require trusted access, such as a documentation page, in a
 * new tab.
 *
 * @param {String} url
 *        The url to open.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openDocLink = async function(url, options) {
  const top = _getTopWindow();
  if (!top) {
    return;
  }
  top.openWebLinkIn(url, "tab", options);
};

/**
 * Opens a |url| controlled by web content in a new tab.
 *
 * If the current tab has an open toolbox, this will attempt to refine the
 * `triggeringPrincipal` of the link using the tab's `contentPrincipal`.  This is only an
 * approximation, so bug 1467945 hopes to improve this.
 *
 * @param {String} url
 *        The url to open.
 * @param {Object} options
 *        Optional parameters, see documentation for openUILinkIn in utilityOverlay.js
 */
exports.openContentLink = async function(url, options = {}) {
  const top = _getTopWindow();
  if (!top) {
    return;
  }
  if (!options.triggeringPrincipal && top.gBrowser) {
    const tab = top.gBrowser.selectedTab;
    if (TabDescriptorFactory.isKnownTab(tab)) {
      options.triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
      options.csp = tab.linkedBrowser.csp;
    }
  }
  top.openWebLinkIn(url, "tab", options);
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
  if (!top) {
    return;
  }
  top.openTrustedLinkIn(url, "tab", options);
};
