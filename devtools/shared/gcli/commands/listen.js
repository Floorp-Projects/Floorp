/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const l10n = require("gcli/l10n");
const { DevToolsLoader } = require("resource://devtools/shared/Loader.jsm");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

const BRAND_SHORT_NAME =
  Services.strings.createBundle("chrome://branding/locale/brand.properties")
                  .GetStringFromName("brandShortName");

XPCOMUtils.defineLazyGetter(this, "debuggerServer", () => {
  // Create a separate loader instance, so that we can be sure to receive
  // a separate instance of the DebuggingServer from the rest of the
  // devtools.  This allows us to safely use the tools against even the
  // actors and DebuggingServer itself, especially since we can mark
  // serverLoader as invisible to the debugger (unlike the usual loader
  // settings).
  let serverLoader = new DevToolsLoader();
  serverLoader.invisibleToDebugger = true;
  // eslint-disable-next-line no-shadow
  let { DebuggerServer: debuggerServer } = serverLoader.require("devtools/server/main");
  debuggerServer.init();
  debuggerServer.registerAllActors();
  debuggerServer.allowChromeProcess = !l10n.hiddenByChromePref();
  return debuggerServer;
});

exports.items = [
  {
    item: "command",
    runAt: "client",
    name: "listen",
    description: l10n.lookup("listenDesc"),
    manual: l10n.lookupFormat("listenManual2", [ BRAND_SHORT_NAME ]),
    params: [
      {
        name: "port",
        type: "number",
        get defaultValue() {
          return Services.prefs.getIntPref("devtools.debugger.remote-port");
        },
        description: l10n.lookup("listenPortDesc"),
      },
      {
        name: "protocol",
        get defaultValue() {
          let webSocket = Services.prefs
                          .getBoolPref("devtools.debugger.remote-websocket");
          let protocol;
          if (webSocket === true) {
            protocol = "websocket";
          } else {
            protocol = "mozilla-rdp";
          }
          return protocol;
        },
        type: {
          name: "selection",
          data: [ "mozilla-rdp", "websocket"],
        },
        description: l10n.lookup("listenProtocolDesc"),
      },
    ],
    exec: function(args, context) {
      let listener = debuggerServer.createListener();
      if (!listener) {
        throw new Error(l10n.lookup("listenDisabledOutput"));
      }

      let webSocket = false;
      if (args.protocol === "websocket") {
        webSocket = true;
      } else if (args.protocol === "mozilla-rdp") {
        webSocket = false;
      }

      listener.portOrPath = args.port;
      listener.webSocket = webSocket;
      listener.open();

      if (debuggerServer.initialized) {
        return l10n.lookupFormat("listenInitOutput", [ "" + args.port ]);
      }

      return l10n.lookup("listenNoInitOutput");
    },
  },
  {
    item: "command",
    runAt: "client",
    name: "unlisten",
    description: l10n.lookup("unlistenDesc"),
    manual: l10n.lookup("unlistenManual"),
    exec: function(args, context) {
      debuggerServer.closeAllListeners();
      return l10n.lookup("unlistenOutput");
    }
  }
];
