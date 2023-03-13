/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
});

export class BrowserTabChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  receiveMessage(message) {
    let context = this.manager.browsingContext;
    let docShell = context.docShell;

    switch (message.name) {
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
          lazy.E10SUtils.wrapHandlingUserInput(
            this.document.defaultView,
            message.data.handlingUserInput,
            () => webNav.reload(reloadFlags)
          );
        } catch (e) {}
        break;

      case "ForceEncodingDetection":
        docShell.forceEncodingDetection();
        break;
    }
  }
}
