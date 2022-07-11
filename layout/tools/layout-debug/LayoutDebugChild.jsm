/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["LayoutDebugChild"];

const NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID =
  "@mozilla.org/layout-debug/layout-debuggingtools;1";

class LayoutDebugChild extends JSWindowActorChild {
  receiveMessage(msg) {
    if (!this._debuggingTools) {
      this._debuggingTools = Cc[
        NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID
      ].createInstance(Ci.nsILayoutDebuggingTools);
      this._debuggingTools.init(this.contentWindow);
    }
    switch (msg.name) {
      case "LayoutDebug:Call":
        let pid = Services.appinfo.processID;
        dump(`[${pid} ${this.contentWindow.location}]\n`);
        this._debuggingTools[msg.data.name](msg.data.arg);
        dump("\n");
        break;
      default:
        throw `unknown message ${msg.name} sent to LayoutDebugChild`;
    }
    return Promise.resolve(true);
  }
}
