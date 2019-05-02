/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SubframeCrashChild"];

let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

class SubframeCrashChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name == "SubframeCrashed") {
      this.onSubframeCrashed(message.data.id);
    }
  }

  onSubframeCrashed(browsingContextId) {
    let bc = BrowsingContext.get(browsingContextId);
    let iframe = bc.embedderElement;
    let uri = Services.io.newURI(iframe.src);
    iframe.removeAttribute("fission");
    iframe.removeAttribute("src");
    // Passing a null remoteType is currently how we tell the iframe to run
    // in the same process as the embedder.
    iframe.changeRemoteness({ remoteType: null });
    let docShell = iframe.frameLoader.docShell;
    docShell.displayLoadError(Cr.NS_ERROR_FRAME_CRASHED, uri, null);
  }
}
