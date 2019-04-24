/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTabChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

class BrowserTabChild extends ActorChild {
  handleEvent(event) {
    switch (event.type) {
    case "DOMWindowCreated":
      let loadContext = this.mm.docShell.QueryInterface(Ci.nsILoadContext);
      let userContextId = loadContext.originAttributes.userContextId;

      this.mm.sendAsyncMessage("Browser:WindowCreated", { userContextId });
      break;

    case "MozAfterPaint":
      this.mm.sendAsyncMessage("Browser:FirstPaint");
      break;

    case "MozDOMPointerLock:Entered":
      this.mm.sendAsyncMessage("PointerLock:Entered", {
        originNoSuffix: event.target.nodePrincipal.originNoSuffix,
      });
      break;

    case "MozDOMPointerLock:Exited":
      this.mm.sendAsyncMessage("PointerLock:Exited");
      break;
    }
  }

  switchDocumentDirection(window = this.content) {
   // document.dir can also be "auto", in which case it won't change
    if (window.document.dir == "ltr" || window.document.dir == "") {
      window.document.dir = "rtl";
    } else if (window.document.dir == "rtl") {
      window.document.dir = "ltr";
    }
    for (let i = 0; i < window.frames.length; i++) {
      this.switchDocumentDirection(window.frames[i]);
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AllowScriptsToClose":
        this.content.windowUtils.allowScriptsToClose();
        break;

      case "Browser:AppTab":
        if (this.docShell) {
          this.docShell.isAppTab = message.data.isAppTab;
        }
        break;

      case "Browser:HasSiblings":
        try {
          let browserChild = this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIBrowserChild);
          let hasSiblings = message.data;
          browserChild.hasSiblings = hasSiblings;
        } catch (e) {
        }
        break;

      // XXX(nika): Should we try to call this in the parent process instead?
      case "Browser:Reload":
        /* First, we'll try to use the session history object to reload so
         * that framesets are handled properly. If we're in a special
         * window (such as view-source) that has no session history, fall
         * back on using the web navigation's reload method.
         */

        let webNav = this.docShell.QueryInterface(Ci.nsIWebNavigation);
        try {
          if (webNav.sessionHistory) {
            webNav = webNav.sessionHistory;
          }
        } catch (e) {
        }

        let reloadFlags = message.data.flags;
        try {
          E10SUtils.wrapHandlingUserInput(this.content, message.data.handlingUserInput,
                                          () => webNav.reload(reloadFlags));
        } catch (e) {
        }
        break;

      case "MixedContent:ReenableProtection":
        this.docShell.mixedContentChannel = null;
        break;

      case "SwitchDocumentDirection":
        this.switchDocumentDirection();
        break;

      case "UpdateCharacterSet":
        this.docShell.charset = message.data.value;
        this.docShell.gatherCharsetMenuTelemetry();
        break;
    }
  }
}
