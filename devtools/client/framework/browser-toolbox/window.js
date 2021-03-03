/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { loader, require, DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);

// Require this module to setup core modules
loader.require("devtools/client/framework/devtools-browser");

var { gDevTools } = require("devtools/client/framework/devtools");
var { Toolbox } = require("devtools/client/framework/toolbox");
var Services = require("Services");
var { DevToolsClient } = require("devtools/client/devtools-client");
var { PrefsHelper } = require("devtools/client/shared/prefs");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);
loader.lazyImporter(
  this,
  "BrowserToolboxLauncher",
  "resource://devtools/client/framework/browser-toolbox/Launcher.jsm"
);

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

var gToolbox, gClient, gShortcuts;

function appendStatusMessage(msg) {
  const statusMessage = document.getElementById("status-message");
  statusMessage.textContent += msg + "\n";
  if (msg.stack) {
    statusMessage.textContent += msg.stack + "\n";
  }
}

function toggleStatusMessage(visible = true) {
  document.getElementById("status-message-container").hidden = !visible;
}

function revealStatusMessage() {
  toggleStatusMessage(true);
}

function hideStatusMessage() {
  toggleStatusMessage(false);
}

var connect = async function() {
  // Initiate the connection
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  // MOZ_BROWSER_TOOLBOX_FISSION_PREF is set by the target Firefox instance
  // before opening the Browser Toolbox.
  // If "devtools.browsertoolbox.fission" is true, the variable is set to "1",
  // otherwise it is set to "0".
  Services.prefs.setBoolPref(
    "devtools.browsertoolbox.fission",
    env.get("MOZ_BROWSER_TOOLBOX_FISSION_PREF") === "1"
  );
  // Similar, but for the WebConsole input context dropdown.
  Services.prefs.setBoolPref(
    "devtools.webconsole.input.context",
    env.get("MOZ_BROWSER_TOOLBOX_INPUT_CONTEXT") === "1"
  );

  const port = env.get("MOZ_BROWSER_TOOLBOX_PORT");

  // A port needs to be passed in from the environment, for instance:
  //    MOZ_BROWSER_TOOLBOX_PORT=6080 ./mach run -chrome \
  //      chrome://devtools/content/framework/browser-toolbox/window.html
  if (!port) {
    throw new Error(
      "Must pass a port in an env variable with MOZ_BROWSER_TOOLBOX_PORT"
    );
  }

  const host = Prefs.chromeDebuggingHost;
  const webSocket = Prefs.chromeDebuggingWebSocket;
  appendStatusMessage(`Connecting to ${host}:${port}, ws: ${webSocket}`);
  const transport = await DevToolsClient.socketConnect({
    host,
    port,
    webSocket,
  });
  gClient = new DevToolsClient(transport);
  appendStatusMessage("Start protocol client for connection");
  await gClient.connect();

  appendStatusMessage("Get root form for toolbox");
  const mainProcessDescriptor = await gClient.mainRoot.getMainProcess();
  await openToolbox(mainProcessDescriptor);
};

// Certain options should be toggled since we can assume chrome debugging here
function setPrefDefaults() {
  Services.prefs.setBoolPref("devtools.inspector.showUserAgentStyles", true);
  Services.prefs.setBoolPref(
    "devtools.performance.ui.show-platform-data",
    true
  );
  Services.prefs.setBoolPref(
    "devtools.inspector.showAllAnonymousContent",
    true
  );
  Services.prefs.setBoolPref("browser.dom.window.dump.enabled", true);
  Services.prefs.setBoolPref("devtools.console.stdout.chrome", true);
  Services.prefs.setBoolPref(
    "devtools.command-button-noautohide.enabled",
    true
  );
  Services.prefs.setBoolPref("devtools.performance.new-panel-enabled", false);
  Services.prefs.setBoolPref("layout.css.emulate-moz-box-with-flex", false);

  Services.prefs.setBoolPref("devtools.performance.enabled", false);
}

window.addEventListener(
  "load",
  async function() {
    gShortcuts = new KeyShortcuts({ window });
    gShortcuts.on("CmdOrCtrl+W", onCloseCommand);
    gShortcuts.on("CmdOrCtrl+Alt+Shift+I", onDebugBrowserToolbox);

    const statusMessageContainer = document.getElementById(
      "status-message-title"
    );
    statusMessageContainer.textContent = L10N.getStr(
      "browserToolbox.statusMessage"
    );

    setPrefDefaults();

    // Reveal status message if connecting is slow or if an error occurs.
    const delayedStatusReveal = setTimeout(
      revealStatusMessage,
      STATUS_REVEAL_TIME
    );
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
  },
  { once: true }
);

function onCloseCommand(event) {
  window.close();
}

/**
 * Open a Browser toolbox debugging the current browser toolbox
 *
 * This helps debugging the browser toolbox code, especially the code
 * running in the parent process. i.e. frontend code.
 */
function onDebugBrowserToolbox() {
  BrowserToolboxLauncher.init();
}

async function openToolbox(descriptorFront) {
  const form = descriptorFront._form;
  appendStatusMessage(
    `Create toolbox for target descriptor: ${JSON.stringify({ form }, null, 2)}`
  );

  // Remember the last panel that was used inside of this profile.
  // But if we are testing, then it should always open the debugger panel.
  const selectedTool = Services.prefs.getCharPref(
    "devtools.browsertoolbox.panel",
    Services.prefs.getCharPref("devtools.toolbox.selectedTool", "jsdebugger")
  );

  const toolboxOptions = { doc: document };
  appendStatusMessage(`Show toolbox with ${selectedTool} selected`);

  gToolbox = await gDevTools.showToolbox(
    descriptorFront,
    selectedTool,
    Toolbox.HostType.BROWSERTOOLBOX,
    toolboxOptions
  );

  bindToolboxHandlers();
  gToolbox.raise();

  // Enable some testing features if the browser toolbox test pref is set.
  if (
    Services.prefs.getBoolPref(
      "devtools.browsertoolbox.enable-test-server",
      false
    )
  ) {
    // setup a server so that the test can evaluate messages in this process.
    installTestingServer();
  }
}

function installTestingServer() {
  // Install a DevToolsServer in this process and inform the server of its
  // location. Tests operating on the browser toolbox run in the server
  // (the firefox parent process) and can connect to this new server using
  // initBrowserToolboxTask(), allowing them to evaluate scripts here.

  const testLoader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { DevToolsServer } = testLoader.require(
    "devtools/server/devtools-server"
  );
  const { SocketListener } = testLoader.require(
    "devtools/shared/security/socket"
  );

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  // Use a fixed port which initBrowserToolboxTask can look for.
  const socketOptions = { portOrPath: 6001 };
  const listener = new SocketListener(DevToolsServer, socketOptions);
  listener.open();
}

async function bindToolboxHandlers() {
  gToolbox.once("destroyed", quitApp);
  window.addEventListener("unload", onUnload);

  if (Services.appinfo.OS == "Darwin") {
    // Badge the dock icon to differentiate this process from the main application
    // process.
    updateBadgeText(false);

    gToolbox.on("toolbox-paused", () => updateBadgeText(true));
    gToolbox.on("toolbox-resumed", () => updateBadgeText(false));
  }
}

function updateBadgeText(paused) {
  const dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"].getService(
    Ci.nsIMacDockSupport
  );
  dockSupport.badgeText = paused ? "▐▐ " : " ▶";
}

function onUnload() {
  window.removeEventListener("unload", onUnload);
  gToolbox.destroy();
}

function quitApp() {
  const quit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(quit, "quit-application-requested");

  const shouldProceed = !quit.data;
  if (shouldProceed) {
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }
}
