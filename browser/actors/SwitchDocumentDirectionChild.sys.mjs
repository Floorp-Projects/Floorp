/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class SwitchDocumentDirectionChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name == "SwitchDocumentDirection") {
      let docShell = this.manager.browsingContext.docShell;
      let document = docShell.QueryInterface(Ci.nsIWebNavigation).document;
      this.switchDocumentDirection(document);
    }
  }

  switchDocumentDirection(document) {
    // document.dir can also be "auto", in which case it won't change
    if (document.dir == "ltr" || document.dir == "") {
      document.dir = "rtl";
    } else if (document.dir == "rtl") {
      document.dir = "ltr";
    }

    for (let frame of document.defaultView.frames) {
      this.switchDocumentDirection(frame.document);
    }
  }
}
