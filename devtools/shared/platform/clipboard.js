/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Helpers for clipboard handling.

"use strict";

const {Cc, Ci} = require("chrome");
const clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper);
const clipboardService = Cc["@mozilla.org/widget/clipboard;1"]
      .getService(Ci.nsIClipboard);

function copyString(string) {
  clipboardHelper.copyString(string);
}

/**
 * Retrieve the current clipboard data matching the flavor "text/unicode".
 *
 * @return {String} Clipboard text content, null if no text clipboard data is available.
 */
function getText() {
  let flavor = "text/unicode";

  let xferable = Cc["@mozilla.org/widget/transferable;1"]
                 .createInstance(Ci.nsITransferable);

  if (!xferable) {
    throw new Error("Couldn't get the clipboard data due to an internal error " +
                    "(couldn't create a Transferable object).");
  }

  xferable.init(null);
  xferable.addDataFlavor(flavor);

  // Get the data into our transferable.
  clipboardService.getData(
    xferable,
    clipboardService.kGlobalClipboard
  );

  let data = {};
  try {
    xferable.getTransferData(flavor, data, {});
  } catch (e) {
    // Clipboard doesn't contain data in flavor, return null.
    return null;
  }

  // There's no data available, return.
  if (!data.value) {
    return null;
  }

  return data.value.QueryInterface(Ci.nsISupportsString).data;
}

exports.copyString = copyString;
exports.getText = getText;
