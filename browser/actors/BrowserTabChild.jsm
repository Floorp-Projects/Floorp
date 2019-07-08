/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTabChild"];

ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);

class BrowserTabChild extends JSWindowActorChild {
  constructor() {
    super();
    this.handledWindowCreated = false;
    this.handledFirstPaint = false;
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMWindowCreated": {
        if (this.handledWindowCreated) {
          return;
        }
        this.handledWindowCreated = true;

        let context = this.manager.browsingContext;
        let loadContext = context.docShell.QueryInterface(Ci.nsILoadContext);
        let userContextId = loadContext.originAttributes.userContextId;

        this.sendAsyncMessage("Browser:WindowCreated", { userContextId });
        break;
      }

      case "MozAfterPaint":
        if (this.handledFirstPaint) {
          return;
        }
        this.handledFirstPaint = true;
        this.sendAsyncMessage("Browser:FirstPaint", {});
        break;

      case "MozDOMPointerLock:Entered":
        this.sendAsyncMessage("PointerLock:Entered", {
          originNoSuffix: event.target.nodePrincipal.originNoSuffix,
        });
        break;

      case "MozDOMPointerLock:Exited":
        this.sendAsyncMessage("PointerLock:Exited");
        break;
    }
  }

  receiveMessage(message) {
    let context = this.manager.browsingContext;
    let docShell = context.docShell;

    switch (message.name) {
      case "Browser:AppTab":
        if (docShell) {
          docShell.isAppTab = message.data.isAppTab;
        }
        break;

      case "Browser:HasSiblings":
        try {
          let browserChild = docShell
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIBrowserChild);
          let hasSiblings = message.data;
          browserChild.hasSiblings = hasSiblings;
        } catch (e) {}
        break;

      // XXX(nika): Should we try to call this in the parent process instead?
      case "Browser:Reload":
        /* First, we'll try to use the session history object to reload so
         * that framesets are handled properly. If we're in a special
         * window (such as view-source) that has no session history, fall
         * back on using the web navigation's reload method.
         */
        let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
        try {
          if (webNav.sessionHistory) {
            webNav = webNav.sessionHistory;
          }
        } catch (e) {}

        let reloadFlags = message.data.flags;
        try {
          E10SUtils.wrapHandlingUserInput(
            this.document.defaultView,
            message.data.handlingUserInput,
            () => webNav.reload(reloadFlags)
          );
        } catch (e) {}
        break;

      case "MixedContent:ReenableProtection":
        docShell.mixedContentChannel = null;
        break;

      case "UpdateCharacterSet":
        docShell.charset = message.data.value;
        docShell.gatherCharsetMenuTelemetry();
        break;
    }
  }
}
