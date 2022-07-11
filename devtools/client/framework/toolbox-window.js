/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable no-unused-vars */

"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// Make some `topBrowsingContext.topChromeWindow` properties available in
// separate devtools window.
//
// (see bug 1659618)
XPCOMUtils.defineLazyScriptGetter(
  this,
  "ZoomManager",
  "chrome://global/content/viewZoomOverlay.js"
);
