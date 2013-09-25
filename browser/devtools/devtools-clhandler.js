/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function devtoolsCommandlineHandler() {
}
devtoolsCommandlineHandler.prototype = {
  handle: function(cmdLine) {
    if (!cmdLine.handleFlag("jsconsole", false)) {
      return;
    }

    Cu.import("resource://gre/modules/Services.jsm");
    let window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      let devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
      // Load the browser devtools main module as the loader's main module.
      devtools.main("main");
      let hudservice = devtools.require("devtools/webconsole/hudservice");
      let console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;
      hudservice.toggleBrowserConsole().then(null, console.error);
    } else {
      window.focus(); // the Browser Console was already open
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  helpInfo : "  -jsconsole         Open the Browser Console.\n",

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([devtoolsCommandlineHandler]);
