/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const EDITORUTILS_CID = Components.ID('{12e63991-86ac-4dff-bb1a-703495d67d17}');

function EditorUtils() {
}

EditorUtils.prototype = {
  classID: EDITORUTILS_CID,
  QueryInterface: ChromeUtils.generateQI([ Ci.nsIEditorUtils ]),

  slurpBlob(aBlob, aScope, aListener) {
    let reader = new aScope.FileReader();
    reader.addEventListener("load", (event) => {
      aListener.onResult(event.target.result);
    });
    reader.addEventListener("error", (event) => {
      aListener.onError(event.target.error.message);
    });

    reader.readAsBinaryString(aBlob);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EditorUtils]);
