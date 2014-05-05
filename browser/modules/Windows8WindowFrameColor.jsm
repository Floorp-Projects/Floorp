/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["Windows8WindowFrameColor"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let Registry = Cu.import("resource://gre/modules/WindowsRegistry.jsm").WindowsRegistry;

const Windows8WindowFrameColor = {
  _windowFrameColor: null,

  get: function() {
    if (this._windowFrameColor)
      return this._windowFrameColor;

    const HKCU = Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER;
    const dwmKey = "Software\\Microsoft\\Windows\\DWM";
    let customizationColor = Registry.readRegKey(HKCU, dwmKey,
                                                 "ColorizationColor");
    // The color returned from the Registry is in decimal form.
    let customizationColorHex = customizationColor.toString(16);
    // Zero-pad the number just to make sure that it is 8 digits.
    customizationColorHex = ("00000000" + customizationColorHex).substr(-8);
    let customizationColorArray = customizationColorHex.match(/../g);
    let [unused, fgR, fgG, fgB] = customizationColorArray.map(function(val) parseInt(val, 16));
    let colorizationColorBalance = Registry.readRegKey(HKCU, dwmKey,
                                                       "ColorizationColorBalance");
     // Window frame base color when Color Intensity is at 0, see bug 1004576.
    let frameBaseColor = 217;
    let alpha = colorizationColorBalance / 100;

    // Alpha-blend the foreground color with the frame base color.
    let r = Math.round(fgR * alpha + frameBaseColor * (1 - alpha));
    let g = Math.round(fgG * alpha + frameBaseColor * (1 - alpha));
    let b = Math.round(fgB * alpha + frameBaseColor * (1 - alpha));
    return this._windowFrameColor = [r, g, b];
  },
};
