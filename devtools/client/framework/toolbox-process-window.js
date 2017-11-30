/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var { loader, require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
// Require this module to setup core modules
loader.require("devtools/client/framework/devtools-browser");

var { gDevTools } = require("devtools/client/framework/devtools");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");
var Services = require("Services");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { PrefsHelper } = require("devtools/client/shared/prefs");
var { Task } = require("devtools/shared/task");

/**
 * Shortcuts for accessing various debugger preferences.
 */
var Prefs = new PrefsHelper("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingWebSocket: ["Bool", "chrome-debugging-websocket"],
});

var gToolbox, gClient;

var connect = Task.async(function* () {
  window.removeEventListener("load", connect);

  // Initiate the connection
  let env = Components.classes["@mozilla.org/process/environment;1"]
    .getService(Components.interfaces.nsIEnvironment);
  let port = env.get("MOZ_BROWSER_TOOLBOX_PORT");
  let addonID = env.get("MOZ_BROWSER_TOOLBOX_ADDONID");

  // A port needs to be passed in from the environment, for instance:
  //    MOZ_BROWSER_TOOLBOX_PORT=6080 ./mach run -chrome \
  //      chrome://devtools/content/framework/toolbox-process-window.xul
  if (!port) {
    throw new Error("Must pass a port in an env variable with MOZ_BROWSER_TOOLBOX_PORT");
  }

  let transport = yield DebuggerClient.socketConnect({
    host: Prefs.chromeDebuggingHost,
    port,
    webSocket: Prefs.chromeDebuggingWebSocket,
  });
  gClient = new DebuggerClient(transport);
  yield gClient.connect();

  if (addonID) {
    let { addons } = yield gClient.listAddons();
    let addonActor = addons.filter(addon => addon.id === addonID).pop();
    let isTabActor = addonActor.isWebExtension;
    openToolbox({form: addonActor, chrome: true, isTabActor});
  } else {
    let response = yield gClient.getProcess();
    openToolbox({form: response.form, chrome: true});
  }
});

// Certain options should be toggled since we can assume chrome debugging here
function setPrefDefaults() {
  Services.prefs.setBoolPref("devtools.inspector.showUserAgentStyles", true);
  Services.prefs.setBoolPref("devtools.performance.ui.show-platform-data", true);
  Services.prefs.setBoolPref("devtools.inspector.showAllAnonymousContent", true);
  Services.prefs.setBoolPref("browser.dom.window.dump.enabled", true);
  Services.prefs.setBoolPref("devtools.command-button-noautohide.enabled", true);
  Services.prefs.setBoolPref("devtools.scratchpad.enabled", true);
  // Bug 1225160 - Using source maps with browser debugging can lead to a crash
  Services.prefs.setBoolPref("devtools.debugger.source-maps-enabled", false);
  Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", true);
  Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", false);
  Services.prefs.setBoolPref("devtools.preference.new-panel-enabled", false);
}
window.addEventListener("load", function () {
  let cmdClose = document.getElementById("toolbox-cmd-close");
  cmdClose.addEventListener("command", onCloseCommand);
  setPrefDefaults();
  connect().catch(e => {
    let errorMessageContainer = document.getElementById("error-message-container");
    let errorMessage = document.getElementById("error-message");
    errorMessage.value = e.message || e;
    errorMessageContainer.hidden = false;
    console.error(e);
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
  TargetFactory.forRemoteTab(options).then(target => {
    let frame = document.getElementById("toolbox-iframe");

    // Remember the last panel that was used inside of this profile.
    // But if we are testing, then it should always open the debugger panel.
    let selectedTool =
      Services.prefs.getCharPref("devtools.browsertoolbox.panel",
        Services.prefs.getCharPref("devtools.toolbox.selectedTool",
                                   "jsdebugger"));

    options = { customIframe: frame };
    gDevTools.showToolbox(target,
                          selectedTool,
                          Toolbox.HostType.CUSTOM,
                          options)
             .then(onNewToolbox);
  });
}

function onNewToolbox(toolbox) {
  gToolbox = toolbox;
  bindToolboxHandlers();
  raise();
  let env = Components.classes["@mozilla.org/process/environment;1"]
    .getService(Components.interfaces.nsIEnvironment);
  let testScript = env.get("MOZ_TOOLBOX_TEST_SCRIPT");
  if (testScript) {
    // Only allow executing random chrome scripts when a special
    // test-only pref is set
    let prefName = "devtools.browser-toolbox.allow-unsafe-script";
    if (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_BOOL &&
        Services.prefs.getBoolPref(prefName) === true) {
      evaluateTestScript(testScript, toolbox);
    }
  }
}

function evaluateTestScript(script, toolbox) {
  let sandbox = Cu.Sandbox(window);
  sandbox.window = window;
  sandbox.toolbox = toolbox;
  Cu.evalInSandbox(script, sandbox);
}

function bindToolboxHandlers() {
  gToolbox.once("destroyed", quitApp);
  window.addEventListener("unload", onUnload);

  if (Services.appinfo.OS == "Darwin") {
    // Badge the dock icon to differentiate this process from the main application
    // process.
    updateBadgeText(false);

    // Once the debugger panel opens listen for thread pause / resume.
    gToolbox.getPanelWhenReady("jsdebugger").then(panel => {
      setupThreadListeners(panel);
    });
  }
}

function setupThreadListeners(panel) {
  updateBadgeText(panel._selectors.getPause(panel._getState()));

  let onPaused = updateBadgeText.bind(null, true);
  let onResumed = updateBadgeText.bind(null, false);
  gToolbox.target.on("thread-paused", onPaused);
  gToolbox.target.on("thread-resumed", onResumed);

  panel.once("destroyed", () => {
    gToolbox.target.off("thread-paused", onPaused);
    gToolbox.target.off("thread-resumed", onResumed);
  });
}

function updateBadgeText(paused) {
  let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
    .getService(Ci.nsIMacDockSupport);
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
  } catch (e) {
    console.error(e);
  }
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
  Services.obs.notifyObservers(quit, "quit-application-requested");

  let shouldProceed = !quit.data;
  if (shouldProceed) {
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }
}
