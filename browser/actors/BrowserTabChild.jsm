/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTabChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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

        this.initializeRPM();

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
        if (message.data.handlingUserInput) {
          reloadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_USER_ACTIVATION;
        }

        try {
          E10SUtils.wrapHandlingUserInput(
            this.document.defaultView,
            message.data.handlingUserInput,
            () => webNav.reload(reloadFlags)
          );
        } catch (e) {}
        break;

      case "UpdateCharacterSet":
        docShell.charset = message.data.value;
        docShell.gatherCharsetMenuTelemetry();
        break;
    }
  }

  // Creates a new PageListener for this process. This will listen for page loads
  // and for those that match URLs provided by the parent process will set up
  // a dedicated message port and notify the parent process.
  // Note: this is part of the old message-manager based RPM and is only still
  // used by newtab and talos tests.
  initializeRPM() {
    // Strip the hash from the URL, because it's not part of the origin.
    let url = this.document.documentURI.replace(/[\#?].*$/, "");

    let registeredURLs = Services.cpmm.sharedData.get("RemotePageManager:urls");

    if (registeredURLs && registeredURLs.has(url)) {
      let { ChildMessagePort } = ChromeUtils.import(
        "resource://gre/modules/remotepagemanager/RemotePageManagerChild.jsm"
      );
      // Set up the child side of the message port
      new ChildMessagePort(this.contentWindow);
    }
  }
}
