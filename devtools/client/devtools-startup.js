/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This XPCOM component is loaded very early.
 * Be careful to lazy load dependencies as much as possible.
 *
 * It manages all the possible entry points for DevTools:
 * - Handles command line arguments like -jsconsole,
 * - Register all key shortcuts,
 * - Listen for "Web Developer" system menu opening, under "Tools",
 * - Inject the wrench icon in toolbar customization, which is used
 *   by the "Web Developer" list displayed in the hamburger menu,
 * - Register the JSON Viewer protocol handler.
 *
 * Only once any of these entry point is fired, this module ensures starting
 * core modules like 'devtools-browser.js' that hooks the browser windows
 * and ensure setting up tools.
 **/

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.chrome.enabled"
];
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableWidgets",
                                  "resource:///modules/CustomizableWidgets.jsm");

XPCOMUtils.defineLazyGetter(this, "Bundle", function () {
  const kUrl = "chrome://devtools/locale/key-shortcuts.properties";
  return Services.strings.createBundle(kUrl);
});

XPCOMUtils.defineLazyGetter(this, "KeyShortcuts", function () {
  const isMac = AppConstants.platform == "macosx";

  // Common modifier shared by most key shortcuts
  const modifiers = isMac ? "accel,alt" : "accel,shift";

  // List of all key shortcuts triggering installation UI
  // `id` should match tool's id from client/definitions.js
  return [
    // The following keys are also registered in /client/menus.js
    // And should be synced.

    // Both are toggling the toolbox on the last selected panel
    // or the default one.
    {
      id: "toggleToolbox",
      shortcut: Bundle.GetStringFromName("toggleToolbox.commandkey"),
      modifiers
    },
    // All locales are using F12
    {
      id: "toggleToolboxF12",
      shortcut: Bundle.GetStringFromName("toggleToolboxF12.commandkey"),
      modifiers: "" // F12 is the only one without modifiers
    },
    // Toggle the visibility of the Developer Toolbar (=gcli)
    {
      id: "toggleToolbar",
      shortcut: Bundle.GetStringFromName("toggleToolbar.commandkey"),
      modifiers: "shift"
    },
    // Open WebIDE window
    {
      id: "webide",
      shortcut: Bundle.GetStringFromName("webide.commandkey"),
      modifiers: "shift"
    },
    // Open the Browser Toolbox
    {
      id: "browserToolbox",
      shortcut: Bundle.GetStringFromName("browserToolbox.commandkey"),
      modifiers: "accel,alt,shift"
    },
    // Open the Browser Console
    {
      id: "browserConsole",
      shortcut: Bundle.GetStringFromName("browserConsole.commandkey"),
      modifiers: "accel,shift"
    },
    // Toggle the Responsive Design Mode
    {
      id: "responsiveDesignMode",
      shortcut: Bundle.GetStringFromName("responsiveDesignMode.commandkey"),
      modifiers
    },
    // Open ScratchPad window
    {
      id: "scratchpad",
      shortcut: Bundle.GetStringFromName("scratchpad.commandkey"),
      modifiers: "shift"
    },

    // The following keys are also registered in /client/definitions.js
    // and should be synced.

    // Key for opening the Inspector
    {
      toolId: "inspector",
      shortcut: Bundle.GetStringFromName("inspector.commandkey"),
      modifiers
    },
    // Key for opening the Web Console
    {
      toolId: "webconsole",
      shortcut: Bundle.GetStringFromName("webconsole.commandkey"),
      modifiers
    },
    // Key for opening the Debugger
    {
      toolId: "jsdebugger",
      shortcut: Bundle.GetStringFromName("debugger.commandkey"),
      modifiers
    },
    // Key for opening the Network Monitor
    {
      toolId: "netmonitor",
      shortcut: Bundle.GetStringFromName("netmonitor.commandkey"),
      modifiers
    },
    // Key for opening the Style Editor
    {
      toolId: "styleeditor",
      shortcut: Bundle.GetStringFromName("styleeditor.commandkey"),
      modifiers: "shift"
    },
    // Key for opening the Performance Panel
    {
      toolId: "performance",
      shortcut: Bundle.GetStringFromName("performance.commandkey"),
      modifiers: "shift"
    },
    // Key for opening the Storage Panel
    {
      toolId: "storage",
      shortcut: Bundle.GetStringFromName("storage.commandkey"),
      modifiers: "shift"
    },
    // Key for opening the DOM Panel
    {
      toolId: "dom",
      shortcut: Bundle.GetStringFromName("dom.commandkey"),
      modifiers
    },
  ];
});

function DevToolsStartup() {}

DevToolsStartup.prototype = {
  handle: function (cmdLine) {
    let consoleFlag = cmdLine.handleFlag("jsconsole", false);
    let debuggerFlag = cmdLine.handleFlag("jsdebugger", false);
    let devtoolsFlag = cmdLine.handleFlag("devtools", false);

    if (consoleFlag) {
      this.handleConsoleFlag(cmdLine);
    }
    if (debuggerFlag) {
      this.handleDebuggerFlag(cmdLine);
    }
    let debuggerServerFlag;
    try {
      debuggerServerFlag =
        cmdLine.handleFlagWithParam("start-debugger-server", false);
    } catch (e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      debuggerServerFlag = cmdLine.handleFlag("start-debugger-server", false);
    }
    if (debuggerServerFlag) {
      this.handleDebuggerServerFlag(cmdLine, debuggerServerFlag);
    }

    let onWindowReady = window => {
      this.hookWindow(window);

      if (devtoolsFlag) {
        this.handleDevToolsFlag(window);
        // This listener is called for all Firefox windows, but we want to execute
        // that command only once
        devtoolsFlag = false;
      }
    };
    Services.obs.addObserver(onWindowReady, "browser-delayed-startup-finished");
  },

  /**
   * Register listeners to all possible entry points for Developer Tools.
   * But instead of implementing the actual actions, defer to DevTools codebase.
   * In most cases, it only needs to call this.initDevTools which handles the rest.
   * We do that to prevent loading any DevTools module until the user intent to use them.
   */
  hookWindow(window) {
    this.hookKeyShortcuts(window);

    // All the other hooks are only necessary if the tools aren't loaded yet.
    if (this.initialized) {
      return;
    }

    this.hookWebDeveloperMenu(window);
    this.hookDeveloperToggle(window);
  },

  /**
   * Dynamically register a wrench icon in the customization menu.
   * You can use this button by right clicking on Firefox toolbar
   * and dragging it from the customization panel to the toolbar.
   * (i.e. this isn't displayed by default to users!)
   *
   * _But_, the "Web Developer" entry in the hamburger menu (the menu with
   * 3 horizontal lines), is using this "developer-button" view to populate
   * its menu. So we have to register this button for the menu to work.
   *
   * Also, this menu duplicates its own entries from the "Web Developer"
   * menu in the system menu, under "Tools" main menu item. The system
   * menu is being hooked by "hookWebDeveloperMenu" which ends up calling
   * devtools/client/framework/browser-menu to create the items for real,
   * initDevTools, from onViewShowing is also calling browser-menu.
   */
  hookDeveloperToggle(window) {
    let id = "developer-button";
    let widget = CustomizableUI.getWidget(id);
    if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
      return;
    }
    let item = {
      id: id,
      type: "view",
      viewId: "PanelUI-developer",
      shortcutId: "key_toggleToolbox",
      tooltiptext: "developer-button.tooltiptext2",
      defaultArea: AppConstants.MOZ_DEV_EDITION ?
                     CustomizableUI.AREA_NAVBAR :
                     CustomizableUI.AREA_PANEL,
      onViewShowing: (event) => {
        // Ensure creating the menuitems in the system menu before trying to copy them.
        this.initDevTools();

        // Populate the subview with whatever menuitems are in the developer
        // menu. We skip menu elements, because the menu panel has no way
        // of dealing with those right now.
        let doc = event.target.ownerDocument;

        let menu = doc.getElementById("menuWebDeveloperPopup");

        let itemsToDisplay = [...menu.children];
        // Hardcode the addition of the "work offline" menuitem at the bottom:
        itemsToDisplay.push({localName: "menuseparator", getAttribute: () => {}});
        itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));

        let developerItems = doc.getElementById("PanelUI-developerItems");
        // Import private helpers from CustomizableWidgets
        let { clearSubview, fillSubviewFromMenuItems } =
          Cu.import("resource:///modules/CustomizableWidgets.jsm", {});
        clearSubview(developerItems);
        fillSubviewFromMenuItems(itemsToDisplay, developerItems);
      },
      onInit(anchor) {
        // Since onBeforeCreated already bails out when initialized, we can call
        // it right away.
        this.onBeforeCreated(anchor.ownerDocument);
      },
      onBeforeCreated(doc) {
        // Bug 1223127, CUI should make this easier to do.
        if (doc.getElementById("PanelUI-developerItems")) {
          return;
        }
        let view = doc.createElement("panelview");
        view.id = "PanelUI-developerItems";
        let panel = doc.createElement("vbox");
        panel.setAttribute("class", "panel-subview-body");
        view.appendChild(panel);
        doc.getElementById("PanelUI-multiView").appendChild(view);
      }
    };
    CustomizableUI.createWidget(item);
    CustomizableWidgets.push(item);
  },

  /*
   * We listen to the "Web Developer" system menu, which is under "Tools" main item.
   * This menu item is hardcoded empty in Firefox UI. We listen for its opening to
   * populate it lazily. Loading main DevTools module is going to populate it.
   */
  hookWebDeveloperMenu(window) {
    let menu = window.document.getElementById("webDeveloperMenu");
    menu.addEventListener("popupshowing", () => this.initDevTools(), { once: true });
  },

  hookKeyShortcuts(window) {
    let doc = window.document;
    let keyset = doc.createElement("keyset");
    keyset.setAttribute("id", "devtoolsKeyset");

    for (let key of KeyShortcuts) {
      let xulKey = this.createKey(doc, key, () => this.onKey(window, key));
      keyset.appendChild(xulKey);
    }

    // Appending a <key> element is not always enough. The <keyset> needs
    // to be detached and reattached to make sure the <key> is taken into
    // account (see bug 832984).
    let mainKeyset = doc.getElementById("mainKeyset");
    mainKeyset.parentNode.insertBefore(keyset, mainKeyset);
  },

  onKey(window, key) {
    let require = this.initDevTools();
    let { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");
    gDevToolsBrowser.onKeyShortcut(window, key);
  },

  // Create a <xul:key> DOM Element
  createKey(doc, { id, toolId, shortcut, modifiers: mod }, oncommand) {
    let k = doc.createElement("key");
    k.id = "key_" + (id || toolId);

    if (shortcut.startsWith("VK_")) {
      k.setAttribute("keycode", shortcut);
    } else {
      k.setAttribute("key", shortcut);
    }

    if (mod) {
      k.setAttribute("modifiers", mod);
    }

    // Bug 371900: command event is fired only if "oncommand" attribute is set.
    k.setAttribute("oncommand", ";");
    k.addEventListener("command", oncommand);

    return k;
  },

  /**
   * Boolean flag to check if DevTools have been already initialized or not.
   * By initialized, we mean that its main modules are loaded.
   */
  initialized: false,

  initDevTools: function () {
    this.initialized = true;
    let { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    // Ensure loading main devtools module that hooks up into browser UI
    // and initialize all devtools machinery.
    require("devtools/client/framework/devtools-browser");
    return require;
  },

  handleConsoleFlag: function (cmdLine) {
    let window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      this.initDevTools();

      let { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
      let hudservice = require("devtools/client/webconsole/hudservice");
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      hudservice.toggleBrowserConsole().catch(console.error);
    } else {
      // the Browser Console was already open
      window.focus();
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  // Open the toolbox on the selected tab once the browser starts up.
  handleDevToolsFlag: function (window) {
    const require = this.initDevTools();
    const {gDevTools} = require("devtools/client/framework/devtools");
    const {TargetFactory} = require("devtools/client/framework/target");
    let target = TargetFactory.forTab(window.gBrowser.selectedTab);
    gDevTools.showToolbox(target);
  },

  _isRemoteDebuggingEnabled() {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every(pref => {
        return Services.prefs.getBoolPref(pref);
      });
    } catch (ex) {
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      console.error(ex);
      return false;
    }
    if (!remoteDebuggingEnabled) {
      let errorMsg = "Could not run chrome debugger! You need the following " +
                     "prefs to be set to true: " + kDebuggerPrefs.join(", ");
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      console.error(new Error(errorMsg));
      // Dump as well, as we're doing this from a commandline, make sure people
      // don't miss it:
      dump(errorMsg + "\n");
    }
    return remoteDebuggingEnabled;
  },

  handleDebuggerFlag: function (cmdLine) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let devtoolsThreadResumed = false;
    let pauseOnStartup = cmdLine.handleFlag("wait-for-jsdebugger", false);
    if (pauseOnStartup) {
      let observe = function (subject, topic, data) {
        devtoolsThreadResumed = true;
        Services.obs.removeObserver(observe, "devtools-thread-resumed");
      };
      Services.obs.addObserver(observe, "devtools-thread-resumed");
    }

    const { BrowserToolboxProcess } = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});
    BrowserToolboxProcess.init();

    if (pauseOnStartup) {
      // Spin the event loop until the debugger connects.
      let tm = Cc["@mozilla.org/thread-manager;1"].getService();
      tm.spinEventLoopUntil(() => {
        return devtoolsThreadResumed;
      });
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /**
   * Handle the --start-debugger-server command line flag. The options are:
   * --start-debugger-server
   *   The portOrPath parameter is boolean true in this case. Reads and uses the defaults
   *   from devtools.debugger.remote-port and devtools.debugger.remote-websocket prefs.
   *   The default values of these prefs are port 6000, WebSocket disabled.
   *
   * --start-debugger-server 6789
   *   Start the non-WebSocket server on port 6789.
   *
   * --start-debugger-server /path/to/filename
   *   Start the server on a Unix domain socket.
   *
   * --start-debugger-server ws:6789
   *   Start the WebSocket server on port 6789.
   *
   * --start-debugger-server ws:
   *   Start the WebSocket server on the default port (taken from d.d.remote-port)
   */
  handleDebuggerServerFlag: function (cmdLine, portOrPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let webSocket = false;
    let defaultPort = Services.prefs.getIntPref("devtools.debugger.remote-port");
    if (portOrPath === true) {
      // Default to pref values if no values given on command line
      webSocket = Services.prefs.getBoolPref("devtools.debugger.remote-websocket");
      portOrPath = defaultPort;
    } else if (portOrPath.startsWith("ws:")) {
      webSocket = true;
      let port = portOrPath.slice(3);
      portOrPath = Number(port) ? port : defaultPort;
    }

    let { DevToolsLoader } =
      Cu.import("resource://devtools/shared/Loader.jsm", {});

    try {
      // Create a separate loader instance, so that we can be sure to receive
      // a separate instance of the DebuggingServer from the rest of the
      // devtools.  This allows us to safely use the tools against even the
      // actors and DebuggingServer itself, especially since we can mark
      // serverLoader as invisible to the debugger (unlike the usual loader
      // settings).
      let serverLoader = new DevToolsLoader();
      serverLoader.invisibleToDebugger = true;
      let { DebuggerServer: debuggerServer } =
        serverLoader.require("devtools/server/main");
      debuggerServer.init();
      debuggerServer.addBrowserActors();
      debuggerServer.allowChromeProcess = true;

      let listener = debuggerServer.createListener();
      listener.portOrPath = portOrPath;
      listener.webSocket = webSocket;
      listener.open();
      dump("Started debugger server on " + portOrPath + "\n");
    } catch (e) {
      dump("Unable to start debugger server on " + portOrPath + ": " + e);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /* eslint-disable max-len */
  helpInfo: "  --jsconsole        Open the Browser Console.\n" +
            "  --jsdebugger       Open the Browser Toolbox.\n" +
            "  --wait-for-jsdebugger Spin event loop until JS debugger connects.\n" +
            "                     Enables debugging (some) application startup code paths.\n" +
            "                     Only has an effect when `--jsdebugger` is also supplied.\n" +
            "  --devtools         Open DevTools on initial load.\n" +
            "  --start-debugger-server [ws:][ <port> | <path> ] Start the debugger server on\n" +
            "                     a TCP port or Unix domain socket path. Defaults to TCP port\n" +
            "                     6000. Use WebSocket protocol if ws: prefix is specified.\n",
  /* eslint-disable max-len */

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [DevToolsStartup]);
