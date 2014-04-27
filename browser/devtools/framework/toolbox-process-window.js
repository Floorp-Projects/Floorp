/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { debuggerSocketConnect, DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { ViewHelpers } =
  Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = new ViewHelpers.Prefs("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingPort: ["Int", "chrome-debugging-port"]
});

let gToolbox, gClient;

function connect() {
  window.removeEventListener("load", connect);
  // Initiate the connection
  let transport = debuggerSocketConnect(
    Prefs.chromeDebuggingHost,
    Prefs.chromeDebuggingPort
  );
  gClient = new DebuggerClient(transport);
  gClient.connect(() => {
    let addonID = getParameterByName("addonID");

    if (addonID) {
      gClient.listAddons(({addons}) => {
        let addonActor = addons.filter(addon => addon.id === addonID).pop();
        openToolbox({ addonActor: addonActor.actor, title: addonActor.name });
      });
    } else {
      gClient.listTabs(openToolbox);
    }
  });
}

window.addEventListener("load", connect);

function openToolbox(form) {
  let options = {
    form: form,
    client: gClient,
    chrome: true
  };
  devtools.TargetFactory.forRemoteTab(options).then(target => {
    let frame = document.getElementById("toolbox-iframe");
    let options = { customIframe: frame };
    gDevTools.showToolbox(target,
                          "jsdebugger",
                          devtools.Toolbox.HostType.CUSTOM,
                          options)
             .then(onNewToolbox);
  });
}

function onNewToolbox(toolbox) {
   gToolbox = toolbox;
   bindToolboxHandlers();
   raise();
}

function bindToolboxHandlers() {
  gToolbox.once("destroyed", quitApp);
  window.addEventListener("unload", onUnload);
}

function onUnload() {
  window.removeEventListener("unload", onUnload);
  window.removeEventListener("message", onMessage);
  gToolbox.destroy();
}

function onMessage(event) {
  try {
    let json = JSON.parse(event.data);
    switch (json.name) {
      case "toolbox-raise":
        raise();
        break;
      case "toolbox-title":
        setTitle(json.data.value);
        break;
    }
  } catch(e) { Cu.reportError(e); }
}

window.addEventListener("message", onMessage);

function raise() {
  window.focus();
}

function setTitle(title) {
  document.title = title;
}

function quitApp() {
  let quit = Cc["@mozilla.org/supports-PRBool;1"]
             .createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(quit, "quit-application-requested", null);

  let shouldProceed = !quit.data;
  if (shouldProceed) {
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }
}

function getParameterByName (name) {
  let name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
  let regex = new RegExp("[\\?&]" + name + "=([^&#]*)");
  let results = regex.exec(window.location.search);
  return results == null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}
