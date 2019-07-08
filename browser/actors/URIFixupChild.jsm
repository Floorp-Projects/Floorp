/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["URIFixupChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

class URIFixupChild extends ActorChild {
  observe(subject) {
    let fixupInfo = subject.QueryInterface(Ci.nsIURIFixupInfo);
    if (!fixupInfo.consumer) {
      return;
    }

    // Ignore info from other docshells
    let parent = fixupInfo.consumer.QueryInterface(Ci.nsIDocShellTreeItem)
      .sameTypeRootTreeItem;
    if (parent != this.mm.docShell) {
      return;
    }

    let data = {};
    for (let f of Object.keys(fixupInfo)) {
      if (f == "consumer" || typeof fixupInfo[f] == "function") {
        continue;
      }

      if (fixupInfo[f] && fixupInfo[f] instanceof Ci.nsIURI) {
        data[f] = fixupInfo[f].spec;
      } else {
        data[f] = fixupInfo[f];
      }
    }

    this.mm.sendAsyncMessage("Browser:URIFixup", data);
  }
}
