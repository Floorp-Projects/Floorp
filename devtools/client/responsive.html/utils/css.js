/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getDOMWindowUtils } = require("./window");

/**
 * Synchronously loads an agent style sheet from `uri` and adds it to the list of
 * additional style sheets of the document.  The sheets added takes effect immediately,
 * and only on the document of the `window` given.
 */
function loadAgentSheet(window, url) {
  const winUtils = getDOMWindowUtils(window);
  winUtils.loadSheetUsingURIString(url, winUtils.AGENT_SHEET);
}
exports.loadAgentSheet = loadAgentSheet;
