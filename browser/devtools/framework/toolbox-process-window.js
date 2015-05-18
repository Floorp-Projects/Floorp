/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { ViewHelpers } =
  Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});
let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = new ViewHelpers.Prefs("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingPort: ["Int", "chrome-debugging-port"]
});

let gToolbox, gClient;

let connect = Task.async(function*() {
  window.removeEventListener("load", connect);
  // Initiate the connection
  let transport = yield DebuggerClient.socketConnect({
    host: Prefs.chromeDebuggingHost,
    port: Prefs.chromeDebuggingPort
  });
  gClient = new DebuggerClient(transport);
  gClient.connect(() => {
    let addonID = getParameterByName("addonID");

    if (addonID) {
      gClient.listAddons(({addons}) => {
        let addonActor = addons.filter(addon => addon.id === addonID).pop();
        openToolbox({ form: addonActor, chrome: true, isTabActor: false });
      });
    } else {
      gClient.getProcess().then(aResponse => {
        openToolbox({ form: aResponse.form, chrome: true });
      });
    }
  });
});

// Certain options should be toggled since we can assume chrome debugging here
function setPrefDefaults() {
  Services.prefs.setBoolPref("devtools.inspector.showUserAgentStyles", true);
  Services.prefs.setBoolPref("devtools.performance.ui.show-platform-data", true);
  Services.prefs.setBoolPref("devtools.inspector.showAllAnonymousContent", true);
  Services.prefs.setBoolPref("browser.dom.window.dump.enabled", true);
}

window.addEventListener("load", function() {
  let cmdClose = document.getElementById("toolbox-cmd-close");
  cmdClose.addEventListener("command", onCloseCommand);
  setPrefDefaults();
  connect().catch(e => {
    let errorMessageContainer = document.getElementById("error-message-container");
    let errorMessage = document.getElementById("error-message");
    errorMessage.value = e;
    errorMessageContainer.hidden = false;
    Cu.reportError(e);
  });
});

function onCloseCommand(event) {
  window.close();
}

function openToolbox({ form, chrome, isTabActor }) {
  let options = {
    form: form,
    client: gClient,
    chrome: chrome,
    isTabActor: isTabActor
  };
  devtools.TargetFactory.forRemoteTab(options).then(target => {
    let frame = document.getElementById("toolbox-iframe");
    let selectedTool = "jsdebugger";

    try {
      // Remember the last panel that was used inside of this profile.
      selectedTool = Services.prefs.getCharPref("devtools.toolbox.selectedTool");
    } catch(e) {}

    try {
      // But if we are testing, then it should always open the debugger panel.
      selectedTool = Services.prefs.getCharPref("devtools.browsertoolbox.panel");
    } catch(e) {}

    let options = { customIframe: frame };
    gDevTools.showToolbox(target,
                          selectedTool,
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

#ifdef XP_MACOSX
  // Badge the dock icon to differentiate this process from the main application process.
  updateBadgeText(false);

  // Once the debugger panel opens listen for thread pause / resume.
  gToolbox.getPanelWhenReady("jsdebugger").then(panel => {
    setupThreadListeners(panel);
  });
#endif
}

function setupThreadListeners(panel) {
  updateBadgeText(panel._controller.activeThread.state == "paused");

  let onPaused = updateBadgeText.bind(null, true);
  let onResumed = updateBadgeText.bind(null, false);
  panel.target.on("thread-paused", onPaused);
  panel.target.on("thread-resumed", onResumed);

  panel.once("destroyed", () => {
    panel.off("thread-paused", onPaused);
    panel.off("thread-resumed", onResumed);
  });
}

function updateBadgeText(paused) {
  let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"].getService(Ci.nsIMacDockSupport);
  dockSupport.badgeText = paused ? "▐▐ " : " ▶";
}

function onUnload() {
  window.removeEventListener("unload", onUnload);
  window.removeEventListener("message", onMessage);
  let cmdClose = document.getElementById("toolbox-cmd-close");
  cmdClose.removeEventListener("command", onCloseCommand);
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
  name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
  let regex = new RegExp("[\\?&]" + name + "=([^&#]*)");
  let results = regex.exec(window.location.search);
  return results == null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}
