/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Helpers for clipboard handling.

"use strict";

const {Cc, Ci} = require("chrome");
const clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper);
var clipboard = require("sdk/clipboard");

function copyString(string) {
  clipboardHelper.copyString(string);
}

function getCurrentFlavors() {
  return clipboard.currentFlavors;
}

function getData() {
  return clipboard.get();
}

exports.copyString = copyString;
exports.getCurrentFlavors = getCurrentFlavors;
exports.getData = getData;
