/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { loader, require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
// Require this module to setup core modules
loader.require("devtools/client/framework/devtools-browser");

var { gDevTools } = require("devtools/client/framework/devtools");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");
var Services = require("Services");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { PrefsHelper } = require("devtools/client/shared/prefs");

// Timeout to wait before we assume that a connect() timed out without an error.
// In milliseconds. (With the Debugger pane open, this has been reported to last
// more than 10 seconds!)
const STATUS_REVEAL_TIME = 15000;

/**
 * Shortcuts for accessing various debugger preferences.
 */
var Prefs = new PrefsHelper("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingWebSocket: ["Bool", "chrome-debugging-websocket"],
});

var gToolbox, gClient;

function appendStatusMessage(msg) {
  let statusMessage = document.getElementById("status-message");
  statusMessage.value += msg + "\n";
  if (msg.stack) {
    statusMessage.value += msg.stack + "\n";
  }
}

function toggleStatusMessage(visible = true) {
  let statusMessageContainer = document.getElementById("status-message-container");
  statusMessageContainer.hidden = !visible;
}

function revealStatusMessage() {
  toggleStatusMessage(true);
}

function hideStatusMessage() {
  toggleStatusMessage(false);
}

var connect = async function() {
  // Initiate the connection
  let env = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment);
  let port = env.get("MOZ_BROWSER_TOOLBOX_PORT");
  let addonID = env.get("MOZ_BROWSER_TOOLBOX_ADDONID");

  // A port needs to be passed in from the environment, for instance:
  //    MOZ_BROWSER_TOOLBOX_PORT=6080 ./mach run -chrome \
  //      chrome://devtools/content/framework/toolbox-process-window.xul
  if (!port) {
    throw new Error("Must pass a port in an env variable with MOZ_BROWSER_TOOLBOX_PORT");
  }

  let host = Prefs.chromeDebuggingHost;
  let webSocket = Prefs.chromeDebuggingWebSocket;
  appendStatusMessage(`Connecting to ${host}:${port}, ws: ${webSocket}`);
  let transport = await DebuggerClient.socketConnect({
    host,
    port,
    webSocket,
  });
  gClient = new DebuggerClient(transport);
  appendStatusMessage("Start protocol client for connection");
  await gClient.connect();

  appendStatusMessage("Get root form for toolbox");
  if (addonID) {
    let { addons } = await gClient.listAddons();
    let addonActor = addons.filter(addon => addon.id === addonID).pop();
    let isTabActor = addonActor.isWebExtension;
    await openToolbox({form: addonActor, chrome: true, isTabActor});
  } else {
    let response = await gClient.getProcess();
    await openToolbox({form: response.form, chrome: true});
  }
};

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
  Services.prefs.setBoolPref("devtools.preference.new-panel-enabled", false);
  Services.prefs.setBoolPref("layout.css.emulate-moz-box-with-flex", false);
}
window.addEventListener("load", async function() {
  let cmdClose = document.getElementById("toolbox-cmd-close");
  cmdClose.addEventListener("command", onCloseCommand);
  setPrefDefaults();
  // Reveal status message if connecting is slow or if an error occurs.
  let delayedStatusReveal = setTimeout(revealStatusMessage, STATUS_REVEAL_TIME);
  try {
    await connect();
    clearTimeout(delayedStatusReveal);
    hideStatusMessage();
  } catch (e) {
    clearTimeout(delayedStatusReveal);
    appendStatusMessage(e);
    revealStatusMessage();
    console.error(e);
  }
}, { once: true });

function onCloseCommand(event) {
  window.close();
}

async function openToolbox({ form, chrome, isTabActor }) {
  let options = {
    form: form,
    client: gClient,
    chrome: chrome,
    isTabActor: isTabActor
  };
  appendStatusMessage(`Create toolbox target: ${JSON.stringify(arguments, null, 2)}`);
  let target = await TargetFactory.forRemoteTab(options);
  let frame = document.getElementById("toolbox-iframe");

  // Remember the last panel that was used inside of this profile.
  // But if we are testing, then it should always open the debugger panel.
  let selectedTool =
    Services.prefs.getCharPref("devtools.browsertoolbox.panel",
      Services.prefs.getCharPref("devtools.toolbox.selectedTool",
                                  "jsdebugger"));

  options = { customIframe: frame };
  appendStatusMessage(`Show toolbox with ${selectedTool} selected`);
  let toolbox = await gDevTools.showToolbox(
    target,
    selectedTool,
    Toolbox.HostType.CUSTOM,
    options
  );
  onNewToolbox(toolbox);
}

function onNewToolbox(toolbox) {
  gToolbox = toolbox;
  bindToolboxHandlers();
  raise();
  let env = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment);
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
  sandbox.ChromeUtils = ChromeUtils;
  Cu.evalInSandbox(script, sandbox);
}

async function bindToolboxHandlers() {
  gToolbox.once("destroyed", quitApp);
  window.addEventListener("unload", onUnload);

  if (Services.appinfo.OS == "Darwin") {
    // Badge the dock icon to differentiate this process from the main application
    // process.
    updateBadgeText(false);

    // Once the debugger panel opens listen for thread pause / resume.
    let panel = await gToolbox.getPanelWhenReady("jsdebugger");
    setupThreadListeners(panel);
  }
}

function setupThreadListeners(panel) {
  updateBadgeText(panel.isPaused());

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
