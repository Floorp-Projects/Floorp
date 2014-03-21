/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["Windows8WindowFrameColor"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/WindowsRegistry.jsm");

const Windows8WindowFrameColor = {
  _windowFrameColor: null,

  get: function() {
    if (this._windowFrameColor)
      return this._windowFrameColor;

    let windowFrameColor = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                                      "Software\\Microsoft\\Windows\\DWM",
                                                      "ColorizationColor");
    // The color returned from the Registry is in decimal form.
    let windowFrameColorHex = windowFrameColor.toString(16);
    // Zero-pad the number just to make sure that it is 8 digits.
    windowFrameColorHex = ("00000000" + windowFrameColorHex).substr(-8);
    let windowFrameColorArray = windowFrameColorHex.match(/../g);
    let [pixelA, pixelR, pixelG, pixelB] = windowFrameColorArray.map(function(val) parseInt(val, 16));

    return this._windowFrameColor = [pixelR, pixelG, pixelB];
  },
};
