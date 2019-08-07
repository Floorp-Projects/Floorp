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
 * - Inject the profiler recording button in toolbar customization.
 *
 * Only once any of these entry point is fired, this module ensures starting
 * core modules like 'devtools-browser.js' that hooks the browser windows
 * and ensure setting up tools.
 **/

"use strict";

const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.chrome.enabled",
];

const DEVTOOLS_ENABLED_PREF = "devtools.enabled";

const DEVTOOLS_POLICY_DISABLED_PREF = "devtools.policy.disabled";
const PROFILER_POPUP_ENABLED_PREF = "devtools.performance.popup.enabled";
const WEBIDE_ENABLED_PREF = "devtools.webide.enabled";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ProfilerMenuButton",
  "resource://devtools/client/performance-new/popup/menu-button.jsm"
);

// We don't want to spend time initializing the full loader here so we create
// our own lazy require.
XPCOMUtils.defineLazyGetter(this, "Telemetry", function() {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  // eslint-disable-next-line no-shadow
  const Telemetry = require("devtools/client/shared/telemetry");

  return Telemetry;
});

XPCOMUtils.defineLazyGetter(this, "StartupBundle", function() {
  const url = "chrome://devtools-startup/locale/startup.properties";
  return Services.strings.createBundle(url);
});

XPCOMUtils.defineLazyGetter(this, "KeyShortcutsBundle", function() {
  const url = "chrome://devtools-startup/locale/key-shortcuts.properties";
  return Services.strings.createBundle(url);
});

XPCOMUtils.defineLazyGetter(this, "KeyShortcuts", function() {
  const isMac = AppConstants.platform == "macosx";

  // Common modifier shared by most key shortcuts
  const modifiers = isMac ? "accel,alt" : "accel,shift";

  // List of all key shortcuts triggering installation UI
  // `id` should match tool's id from client/definitions.js
  const shortcuts = [
    // The following keys are also registered in /client/menus.js
    // And should be synced.

    // Both are toggling the toolbox on the last selected panel
    // or the default one.
    {
      id: "toggleToolbox",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "toggleToolbox.commandkey"
      ),
      modifiers,
    },
    // All locales are using F12
    {
      id: "toggleToolboxF12",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "toggleToolboxF12.commandkey"
      ),
      modifiers: "", // F12 is the only one without modifiers
    },
    // Open the Browser Toolbox
    {
      id: "browserToolbox",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "browserToolbox.commandkey"
      ),
      modifiers: "accel,alt,shift",
    },
    // Open the Browser Console
    {
      id: "browserConsole",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "browserConsole.commandkey"
      ),
      modifiers: "accel,shift",
    },
    // Toggle the Responsive Design Mode
    {
      id: "responsiveDesignMode",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "responsiveDesignMode.commandkey"
      ),
      modifiers,
    },
    // Open ScratchPad window
    {
      id: "scratchpad",
      shortcut: KeyShortcutsBundle.GetStringFromName("scratchpad.commandkey"),
      modifiers: "shift",
    },

    // The following keys are also registered in /client/definitions.js
    // and should be synced.

    // Key for opening the Inspector
    {
      toolId: "inspector",
      shortcut: KeyShortcutsBundle.GetStringFromName("inspector.commandkey"),
      modifiers,
    },
    // Key for opening the Web Console
    {
      toolId: "webconsole",
      shortcut: KeyShortcutsBundle.GetStringFromName("webconsole.commandkey"),
      modifiers,
    },
    // Key for opening the Network Monitor
    {
      toolId: "netmonitor",
      shortcut: KeyShortcutsBundle.GetStringFromName("netmonitor.commandkey"),
      modifiers,
    },
    // Key for opening the Style Editor
    {
      toolId: "styleeditor",
      shortcut: KeyShortcutsBundle.GetStringFromName("styleeditor.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the Performance Panel
    {
      toolId: "performance",
      shortcut: KeyShortcutsBundle.GetStringFromName("performance.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the Storage Panel
    {
      toolId: "storage",
      shortcut: KeyShortcutsBundle.GetStringFromName("storage.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the DOM Panel
    {
      toolId: "dom",
      shortcut: KeyShortcutsBundle.GetStringFromName("dom.commandkey"),
      modifiers,
    },
    // Key for opening the Accessibility Panel
    {
      toolId: "accessibility",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "accessibilityF12.commandkey"
      ),
      modifiers: "shift",
    },
  ];

  if (isMac) {
    // Add the extra key command for macOS, so you can open the inspector with cmd+shift+C
    // like on Chrome DevTools.
    shortcuts.push({
      id: "inspectorMac",
      toolId: "inspector",
      shortcut: KeyShortcutsBundle.GetStringFromName("inspector.commandkey"),
      modifiers: "accel,shift",
    });
  }

  // Only add the WebIDE shortcut if WebIDE is enabled.
  const isWebIDEEnabled = Services.prefs.getBoolPref(
    WEBIDE_ENABLED_PREF,
    false
  );
  if (isWebIDEEnabled) {
    // Open WebIDE window
    shortcuts.push({
      id: "webide",
      shortcut: KeyShortcutsBundle.GetStringFromName("webide.commandkey"),
      modifiers: "shift",
    });
  }

  if (isProfilerButtonEnabled()) {
    shortcuts.push(...getProfilerKeyShortcuts());
  }

  return shortcuts;
});

function getProfilerKeyShortcuts() {
  return [
    // Start/stop the profiler
    {
      id: "profilerStartStop",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "profilerStartStop.commandkey"
      ),
      modifiers: "control,shift",
    },
    // Capture a profile
    {
      id: "profilerCapture",
      shortcut: KeyShortcutsBundle.GetStringFromName(
        "profilerCapture.commandkey"
      ),
      modifiers: "control,shift",
    },
  ];
}

/**
 * Instead of loading the ProfilerMenuButton.jsm file, provide an independent check
 * to see if it is turned on.
 */
function isProfilerButtonEnabled() {
  return Services.prefs.getBoolPref(PROFILER_POPUP_ENABLED_PREF, false);
}

XPCOMUtils.defineLazyGetter(this, "ProfilerPopupBackground", function() {
  return ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm"
  );
});

function DevToolsStartup() {
  this.onEnabledPrefChanged = this.onEnabledPrefChanged.bind(this);
  this.onWindowReady = this.onWindowReady.bind(this);
  this.toggleProfilerKeyShortcuts = this.toggleProfilerKeyShortcuts.bind(this);
}

DevToolsStartup.prototype = {
  /**
   * Boolean flag to check if DevTools have been already initialized or not.
   * By initialized, we mean that its main modules are loaded.
   */
  initialized: false,

  /**
   * Boolean flag to check if the devtools initialization was already sent to telemetry.
   * We only want to record one devtools entry point per Firefox run, but we are not
   * interested in all the entry points.
   */
  recorded: false,

  get telemetry() {
    if (!this._telemetry) {
      this._telemetry = new Telemetry();
      this._telemetry.setEventRecordingEnabled(true);
    }
    return this._telemetry;
  },

  /**
   * Flag that indicates if the developer toggle was already added to customizableUI.
   */
  developerToggleCreated: false,

  /**
   * Flag that indicates if the profiler recording popup was already added to
   * customizableUI.
   */
  profilerRecordingButtonCreated: false,

  isDisabledByPolicy: function() {
    return Services.prefs.getBoolPref(DEVTOOLS_POLICY_DISABLED_PREF, false);
  },

  handle: function(cmdLine) {
    const flags = this.readCommandLineFlags(cmdLine);

    // handle() can be called after browser startup (e.g. opening links from other apps).
    const isInitialLaunch =
      cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH;
    if (isInitialLaunch) {
      // Enable devtools for all users on startup (onboarding experiment from Bug 1408969
      // is over).
      Services.prefs.setBoolPref(DEVTOOLS_ENABLED_PREF, true);

      // Store devtoolsFlag to check it later in onWindowReady.
      this.devtoolsFlag = flags.devtools;
      // Only top level Firefox Windows fire a browser-delayed-startup-finished event
      Services.obs.addObserver(
        this.onWindowReady,
        "browser-delayed-startup-finished"
      );

      if (!this.isDisabledByPolicy()) {
        if (AppConstants.MOZ_DEV_EDITION) {
          // On DevEdition, the developer toggle is displayed by default in the navbar
          // area and should be created before the first paint.
          this.hookDeveloperToggle();
        }

        this.hookProfilerRecordingButton();
      }

      // Update menu items when devtools.enabled changes.
      Services.prefs.addObserver(
        DEVTOOLS_ENABLED_PREF,
        this.onEnabledPrefChanged
      );
    }

    if (flags.console) {
      this.commandLine = true;
      this.handleConsoleFlag(cmdLine);
    }
    if (flags.debugger) {
      this.commandLine = true;
      this.handleDebuggerFlag(cmdLine);
    }

    if (flags.debuggerServer) {
      this.handleDebuggerServerFlag(cmdLine, flags.debuggerServer);
    }
  },

  readCommandLineFlags(cmdLine) {
    // All command line flags are disabled if DevTools are disabled by policy.
    if (this.isDisabledByPolicy()) {
      return {
        console: false,
        debugger: false,
        devtools: false,
        debuggerServer: false,
      };
    }

    const console = cmdLine.handleFlag("jsconsole", false);
    const debuggerFlag = cmdLine.handleFlag("jsdebugger", false);
    const devtools = cmdLine.handleFlag("devtools", false);

    let debuggerServer;
    try {
      debuggerServer = cmdLine.handleFlagWithParam(
        "start-debugger-server",
        false
      );
    } catch (e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      debuggerServer = cmdLine.handleFlag("start-debugger-server", false);
    }

    return { console, debugger: debuggerFlag, devtools, debuggerServer };
  },

  /**
   * Called when receiving the "browser-delayed-startup-finished" event for a new
   * top-level window.
   */
  onWindowReady(window) {
    if (this.isDisabledByPolicy()) {
      this.removeDevToolsMenus(window);
      return;
    }

    this.hookWindow(window);

    // This listener is called for all Firefox windows, but we want to execute some code
    // only once.
    if (!this._firstWindowReadyReceived) {
      this.onFirstWindowReady(window);
      this._firstWindowReadyReceived = true;
    }

    JsonView.initialize();
  },

  removeDevToolsMenus(window) {
    // This will hide the "Tools > Web Developer" menu.
    window.document
      .getElementById("webDeveloperMenu")
      .setAttribute("hidden", "true");
    // This will hide the "Web Developer" item in the hamburger menu.
    window.document
      .getElementById("appMenu-developer-button")
      .setAttribute("hidden", "true");
  },

  onFirstWindowReady(window) {
    if (this.devtoolsFlag) {
      this.handleDevToolsFlag(window);

      // In the case of the --jsconsole and --jsdebugger command line parameters
      // there was no browser window when they were processed so we act on the
      // this.commandline flag instead.
      if (this.commandLine) {
        this.sendEntryPointTelemetry("CommandLine");
      }
    }
  },

  /**
   * Register listeners to all possible entry points for Developer Tools.
   * But instead of implementing the actual actions, defer to DevTools codebase.
   * In most cases, it only needs to call this.initDevTools which handles the rest.
   * We do that to prevent loading any DevTools module until the user intent to use them.
   */
  hookWindow(window) {
    // Key Shortcuts need to be added on all the created windows.
    this.hookKeyShortcuts(window);

    // In some situations (e.g. starting Firefox with --jsconsole) DevTools will be
    // initialized before the first browser-delayed-startup-finished event is received.
    // We use a dedicated flag because we still need to hook the developer toggle.
    this.hookDeveloperToggle();
    this.hookProfilerRecordingButton();

    // The developer menu hook only needs to be added if devtools have not been
    // initialized yet.
    if (!this.initialized) {
      this.hookWebDeveloperMenu(window);
    }

    this.createDevToolsEnableMenuItem(window);
    this.updateDevToolsMenuItems(window);
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
  hookDeveloperToggle() {
    if (this.developerToggleCreated) {
      return;
    }

    const id = "developer-button";
    const widget = CustomizableUI.getWidget(id);
    if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
      return;
    }
    const item = {
      id: id,
      type: "view",
      viewId: "PanelUI-developer",
      shortcutId: "key_toggleToolbox",
      tooltiptext: "developer-button.tooltiptext2",
      onViewShowing: event => {
        if (Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
          // If DevTools are enabled, initialize DevTools to create all menuitems in the
          // system menu before trying to copy them.
          this.initDevTools("HamburgerMenu");
        }

        // Populate the subview with whatever menuitems are in the developer
        // menu. We skip menu elements, because the menu panel has no way
        // of dealing with those right now.
        const doc = event.target.ownerDocument;

        const menu = doc.getElementById("menuWebDeveloperPopup");

        const itemsToDisplay = [...menu.children];
        // Hardcode the addition of the "work offline" menuitem at the bottom:
        itemsToDisplay.push({
          localName: "menuseparator",
          getAttribute: () => {},
        });
        itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));

        const developerItems = doc.getElementById("PanelUI-developerItems");
        CustomizableUI.clearSubview(developerItems);
        CustomizableUI.fillSubviewFromMenuItems(itemsToDisplay, developerItems);
      },
      onInit(anchor) {
        // Since onBeforeCreated already bails out when initialized, we can call
        // it right away.
        this.onBeforeCreated(anchor.ownerDocument);
      },
      onBeforeCreated: doc => {
        // The developer toggle needs the "key_toggleToolbox" <key> element.
        // In DEV EDITION, the toggle is added before 1st paint and hookKeyShortcuts() is
        // not called yet when CustomizableUI creates the widget.
        this.hookKeyShortcuts(doc.defaultView);

        // Bug 1223127, CUI should make this easier to do.
        if (doc.getElementById("PanelUI-developerItems")) {
          return;
        }
        const view = doc.createXULElement("panelview");
        view.id = "PanelUI-developerItems";
        const panel = doc.createXULElement("vbox");
        panel.setAttribute("class", "panel-subview-body");
        view.appendChild(panel);
        doc.getElementById("PanelUI-multiView").appendChild(view);
      },
    };
    CustomizableUI.createWidget(item);
    CustomizableWidgets.push(item);

    this.developerToggleCreated = true;
  },

  /**
   * Dynamically register a profiler recording button in the
   * customization menu. You can use this button by right clicking
   * on Firefox toolbar and dragging it from the customization panel
   * to the toolbar. (i.e. this isn't displayed by default to users.)
   */
  hookProfilerRecordingButton() {
    if (this.profilerRecordingButtonCreated) {
      return;
    }
    this.profilerRecordingButtonCreated = true;
    if (isProfilerButtonEnabled()) {
      ProfilerMenuButton.initialize();
    }
  },

  /*
   * We listen to the "Web Developer" system menu, which is under "Tools" main item.
   * This menu item is hardcoded empty in Firefox UI. We listen for its opening to
   * populate it lazily. Loading main DevTools module is going to populate it.
   */
  hookWebDeveloperMenu(window) {
    const menu = window.document.getElementById("webDeveloperMenu");
    const onPopupShowing = () => {
      if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
        return;
      }
      menu.removeEventListener("popupshowing", onPopupShowing);
      this.initDevTools("SystemMenu");
    };
    menu.addEventListener("popupshowing", onPopupShowing);
  },

  /**
   * Create a new menu item to enable DevTools and insert it DevTools's submenu in the
   * System Menu.
   */
  createDevToolsEnableMenuItem(window) {
    const { document } = window;

    // Create the menu item.
    const item = document.createXULElement("menuitem");
    item.id = "enableDeveloperTools";
    item.setAttribute(
      "label",
      StartupBundle.GetStringFromName("enableDevTools.label")
    );
    item.setAttribute(
      "accesskey",
      StartupBundle.GetStringFromName("enableDevTools.accesskey")
    );

    // The menu item should open the install page for DevTools.
    item.addEventListener("command", () => {
      this.openInstallPage("SystemMenu");
    });

    // Insert the menu item in the DevTools submenu.
    const systemMenuItem = document.getElementById("menuWebDeveloperPopup");
    systemMenuItem.appendChild(item);
  },

  /**
   * Update the visibility the menu item to enable DevTools.
   */
  updateDevToolsMenuItems(window) {
    const item = window.document.getElementById("enableDeveloperTools");
    item.hidden = Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF);
  },

  /**
   * Loop on all windows and update the hidden attribute of the "enable DevTools" menu
   * item.
   */
  onEnabledPrefChanged() {
    for (const window of Services.wm.getEnumerator("navigator:browser")) {
      if (window.gBrowserInit && window.gBrowserInit.delayedStartupFinished) {
        this.updateDevToolsMenuItems(window);
      }
    }
  },

  /**
   * Check if the user is a DevTools user by looking at our selfxss pref.
   * This preference is incremented everytime the console is used (up to 5).
   *
   * @return {Boolean} true if the user can be considered as a devtools user.
   */
  isDevToolsUser() {
    const selfXssCount = Services.prefs.getIntPref("devtools.selfxss.count", 0);
    return selfXssCount > 0;
  },

  hookKeyShortcuts(window) {
    const doc = window.document;

    // hookKeyShortcuts can be called both from hookWindow and from the developer toggle
    // onBeforeCreated. Make sure shortcuts are only added once per window.
    if (doc.getElementById("devtoolsKeyset")) {
      return;
    }

    const keyset = doc.createXULElement("keyset");
    keyset.setAttribute("id", "devtoolsKeyset");

    this.attachKeys(doc, KeyShortcuts, keyset);

    // Appending a <key> element is not always enough. The <keyset> needs
    // to be detached and reattached to make sure the <key> is taken into
    // account (see bug 832984).
    const mainKeyset = doc.getElementById("mainKeyset");
    mainKeyset.parentNode.insertBefore(keyset, mainKeyset);

    // Watch for the profiler to enable or disable the profiler popup, then toggle
    // the keyboard shortcuts on and off.
    Services.prefs.addObserver(
      PROFILER_POPUP_ENABLED_PREF,
      this.toggleProfilerKeyShortcuts
    );
  },

  /**
   * This method attaches on the key elements to the devtools keyset.
   */
  attachKeys(doc, keyShortcuts, keyset = doc.getElementById("devtoolsKeyset")) {
    const window = doc.defaultView;
    for (const key of keyShortcuts) {
      const xulKey = this.createKey(doc, key, () => this.onKey(window, key));
      keyset.appendChild(xulKey);
    }
  },

  /**
   * This method removes keys from the devtools keyset.
   */
  removeKeys(doc, keyShortcuts) {
    for (const key of keyShortcuts) {
      const keyElement = doc.getElementById(this.getKeyElementId(key));
      if (keyElement) {
        keyElement.remove();
      }
    }
  },

  /**
   * We only want to have the keyboard shortcuts active when the menu button is on.
   * This function either adds or removes the elements.
   */
  toggleProfilerKeyShortcuts() {
    const isEnabled = isProfilerButtonEnabled();
    const profilerKeyShortcuts = getProfilerKeyShortcuts();
    for (const { document } of Services.wm.getEnumerator(null)) {
      const devtoolsKeyset = document.getElementById("devtoolsKeyset");
      const mainKeyset = document.getElementById("mainKeyset");

      if (!devtoolsKeyset || !mainKeyset) {
        // There may not be devtools keyset on this window.
        continue;
      }

      if (isEnabled) {
        this.attachKeys(document, profilerKeyShortcuts);
      } else {
        this.removeKeys(document, profilerKeyShortcuts);
      }
      // Appending a <key> element is not always enough. The <keyset> needs
      // to be detached and reattached to make sure the <key> is taken into
      // account (see bug 832984).
      mainKeyset.parentNode.insertBefore(devtoolsKeyset, mainKeyset);
    }

    if (!isEnabled) {
      // Ensure the profiler isn't left profiling in the background.
      ProfilerPopupBackground.stopProfiler();
    }
  },

  async onKey(window, key) {
    try {
      // The profiler doesn't care if DevTools is loaded, so provide a quick check
      // first to bail out of checking if DevTools is available.
      switch (key.id) {
        case "profilerStartStop": {
          ProfilerPopupBackground.toggleProfiler();
          return;
        }
        case "profilerCapture": {
          ProfilerPopupBackground.captureProfile();
          return;
        }
      }
      if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
        const id = key.toolId || key.id;
        this.openInstallPage("KeyShortcut", id);
      } else {
        // Record the timing at which this event started in order to compute later in
        // gDevTools.showToolbox, the complete time it takes to open the toolbox.
        // i.e. especially take `initDevTools` into account.
        const startTime = Cu.now();
        const require = this.initDevTools("KeyShortcut", key);
        const {
          gDevToolsBrowser,
        } = require("devtools/client/framework/devtools-browser");
        await gDevToolsBrowser.onKeyShortcut(window, key, startTime);
      }
    } catch (e) {
      console.error(`Exception while trigerring key ${key}: ${e}\n${e.stack}`);
    }
  },

  getKeyElementId({ id, toolId }) {
    return "key_" + (id || toolId);
  },

  // Create a <xul:key> DOM Element
  createKey(doc, key, oncommand) {
    const { shortcut, modifiers: mod } = key;
    const k = doc.createXULElement("key");
    k.id = this.getKeyElementId(key);

    if (shortcut.startsWith("VK_")) {
      k.setAttribute("keycode", shortcut);
      if (shortcut.match(/^VK_\d$/)) {
        // Add the event keydown attribute to ensure that shortcuts work for combinations
        // such as ctrl shift 1.
        k.setAttribute("event", "keydown");
      }
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

  initDevTools: function(reason, key = "") {
    // If an entry point is fired and tools are not enabled open the installation page
    if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
      this.openInstallPage(reason);
      return null;
    }

    // In the case of the --jsconsole and --jsdebugger command line parameters
    // there is no browser window yet so we don't send any telemetry yet.
    if (reason !== "CommandLine") {
      this.sendEntryPointTelemetry(reason, key);
    }

    this.initialized = true;
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    // Ensure loading main devtools module that hooks up into browser UI
    // and initialize all devtools machinery.
    require("devtools/client/framework/devtools-browser");
    return require;
  },

  /**
   * Open about:devtools to start the onboarding flow.
   *
   * @param {String} reason
   *        One of "KeyShortcut", "SystemMenu", "HamburgerMenu", "ContextMenu",
   *        "CommandLine".
   * @param {String} keyId
   *        Optional. If the onboarding flow was triggered by a keyboard shortcut, pass
   *        the shortcut key id (or toolId) to about:devtools.
   */
  openInstallPage: function(reason, keyId) {
    // If DevTools are completely disabled, bail out here as this might be called directly
    // from other files.
    if (this.isDisabledByPolicy()) {
      return;
    }

    const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");

    // Focus about:devtools tab if there is already one opened in the current window.
    for (const tab of gBrowser.tabs) {
      const browser = tab.linkedBrowser;
      // browser.documentURI might be undefined if the browser tab is still loading.
      const location = browser.documentURI ? browser.documentURI.spec : "";
      if (
        location.startsWith("about:devtools") &&
        !location.startsWith("about:devtools-toolbox")
      ) {
        // Focus the existing about:devtools tab and bail out.
        gBrowser.selectedTab = tab;
        return;
      }
    }

    let url = "about:devtools";

    const params = [];
    if (reason) {
      params.push("reason=" + encodeURIComponent(reason));
    }

    const selectedBrowser = gBrowser.selectedBrowser;
    if (selectedBrowser) {
      params.push("tabid=" + selectedBrowser.outerWindowID);
    }

    if (keyId) {
      params.push("keyid=" + keyId);
    }

    if (params.length > 0) {
      url += "?" + params.join("&");
    }

    // Set relatedToCurrent: true to open the tab next to the current one.
    gBrowser.selectedTab = gBrowser.addTrustedTab(url, {
      relatedToCurrent: true,
    });
  },

  handleConsoleFlag: function(cmdLine) {
    const window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      const require = this.initDevTools("CommandLine");
      const {
        BrowserConsoleManager,
      } = require("devtools/client/webconsole/browser-console-manager");
      BrowserConsoleManager.toggleBrowserConsole().catch(console.error);
    } else {
      // the Browser Console was already open
      window.focus();
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  // Open the toolbox on the selected tab once the browser starts up.
  handleDevToolsFlag: async function(window) {
    const require = this.initDevTools("CommandLine");
    const { gDevTools } = require("devtools/client/framework/devtools");
    const { TargetFactory } = require("devtools/client/framework/target");
    const target = await TargetFactory.forTab(window.gBrowser.selectedTab);
    gDevTools.showToolbox(target);
  },

  _isRemoteDebuggingEnabled() {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every(pref => {
        return Services.prefs.getBoolPref(pref);
      });
    } catch (ex) {
      console.error(ex);
      return false;
    }
    if (!remoteDebuggingEnabled) {
      const errorMsg =
        "Could not run chrome debugger! You need the following " +
        "prefs to be set to true: " +
        kDebuggerPrefs.join(", ");
      console.error(new Error(errorMsg));
      // Dump as well, as we're doing this from a commandline, make sure people
      // don't miss it:
      dump(errorMsg + "\n");
    }
    return remoteDebuggingEnabled;
  },

  handleDebuggerFlag: function(cmdLine) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let devtoolsThreadResumed = false;
    const pauseOnStartup = cmdLine.handleFlag("wait-for-jsdebugger", false);
    if (pauseOnStartup) {
      const observe = function(subject, topic, data) {
        devtoolsThreadResumed = true;
        Services.obs.removeObserver(observe, "devtools-thread-resumed");
      };
      Services.obs.addObserver(observe, "devtools-thread-resumed");
    }

    const { BrowserToolboxProcess } = ChromeUtils.import(
      "resource://devtools/client/framework/ToolboxProcess.jsm"
    );
    BrowserToolboxProcess.init();

    if (pauseOnStartup) {
      // Spin the event loop until the debugger connects.
      const tm = Cc["@mozilla.org/thread-manager;1"].getService();
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
  handleDebuggerServerFlag: function(cmdLine, portOrPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let webSocket = false;
    const defaultPort = Services.prefs.getIntPref(
      "devtools.debugger.remote-port"
    );
    if (portOrPath === true) {
      // Default to pref values if no values given on command line
      webSocket = Services.prefs.getBoolPref(
        "devtools.debugger.remote-websocket"
      );
      portOrPath = defaultPort;
    } else if (portOrPath.startsWith("ws:")) {
      webSocket = true;
      const port = portOrPath.slice(3);
      portOrPath = Number(port) ? port : defaultPort;
    }

    const { DevToolsLoader } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );

    try {
      // Create a separate loader instance, so that we can be sure to receive
      // a separate instance of the DebuggingServer from the rest of the
      // devtools.  This allows us to safely use the tools against even the
      // actors and DebuggingServer itself, especially since we can mark
      // serverLoader as invisible to the debugger (unlike the usual loader
      // settings).
      const serverLoader = new DevToolsLoader({
        invisibleToDebugger: true,
      });
      const { DebuggerServer: debuggerServer } = serverLoader.require(
        "devtools/server/main"
      );
      const { SocketListener } = serverLoader.require(
        "devtools/shared/security/socket"
      );
      debuggerServer.init();
      debuggerServer.registerAllActors();
      debuggerServer.allowChromeProcess = true;
      const socketOptions = { portOrPath, webSocket };

      const listener = new SocketListener(debuggerServer, socketOptions);
      listener.open();
      dump("Started debugger server on " + portOrPath + "\n");
    } catch (e) {
      dump("Unable to start debugger server on " + portOrPath + ": " + e);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /**
   * Send entry point telemetry explaining how the devtools were launched. This
   * functionality also lives inside `devtools/client/framework/browser-menus.js`
   * because this codepath is only used the first time a toolbox is opened for a
   * tab.
   *
   * @param {String} reason
   *        One of "KeyShortcut", "SystemMenu", "HamburgerMenu", "ContextMenu",
   *        "CommandLine".
   * @param {String} key
   *        The key used by a key shortcut.
   */
  sendEntryPointTelemetry(reason, key = "") {
    if (!reason) {
      return;
    }

    let keys = "";

    if (reason === "KeyShortcut") {
      let { modifiers, shortcut } = key;

      modifiers = modifiers.replace(",", "+");

      if (shortcut.startsWith("VK_")) {
        shortcut = shortcut.substr(3);
      }

      keys = `${modifiers}+${shortcut}`;
    }

    const window = Services.wm.getMostRecentWindow("navigator:browser");

    this.telemetry.addEventProperty(
      window,
      "open",
      "tools",
      null,
      "shortcut",
      keys
    );
    this.telemetry.addEventProperty(
      window,
      "open",
      "tools",
      null,
      "entrypoint",
      reason
    );

    if (this.recorded) {
      return;
    }

    // Only save the first call for each firefox run as next call
    // won't necessarely start the tool. For example key shortcuts may
    // only change the currently selected tool.
    try {
      this.telemetry.getHistogramById("DEVTOOLS_ENTRY_POINT").add(reason);
    } catch (e) {
      dump("DevTools telemetry entry point failed: " + e + "\n");
    }
    this.recorded = true;
  },

  // Used by tests and the toolbox to register the same key shortcuts in toolboxes loaded
  // in a window window.
  get KeyShortcuts() {
    return KeyShortcuts;
  },
  get wrappedJSObject() {
    return this;
  },

  /* eslint-disable max-len */
  helpInfo:
    "  --jsconsole        Open the Browser Console.\n" +
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsICommandLineHandler]),
};

/**
 * Singleton object that represents the JSON View in-content tool.
 * It has the same lifetime as the browser.
 */
const JsonView = {
  initialized: false,

  initialize: function() {
    // Prevent loading the frame script multiple times if we call this more than once.
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    // Load JSON converter module. This converter is responsible
    // for handling 'application/json' documents and converting
    // them into a simple web-app that allows easy inspection
    // of the JSON data.
    Services.ppmm.loadProcessScript(
      "resource://devtools/client/jsonview/converter-observer.js",
      true
    );

    // Register for messages coming from the child process.
    // This is never removed as there is no particular need to unregister
    // it during shutdown.
    Services.ppmm.addMessageListener("devtools:jsonview:save", this.onSave);
  },

  // Message handlers for events from child processes

  /**
   * Save JSON to a file needs to be implemented here
   * in the parent process.
   */
  onSave: function(message) {
    const chrome = Services.wm.getMostRecentWindow("navigator:browser");
    const browser = chrome.gBrowser.selectedBrowser;
    if (message.data === null) {
      // Save original contents
      chrome.saveBrowser(browser);
    } else {
      if (
        !message.data.startsWith("blob:null") ||
        !browser.contentPrincipal.isNullPrincipal
      ) {
        Cu.reportError("Got invalid request to save JSON data");
        return;
      }
      // The following code emulates saveBrowser, but:
      // - Uses the given blob URL containing the custom contents to save.
      // - Obtains the file name from the URL of the document, not the blob.
      // - avoids passing the document and explicitly passes system principal.
      //   We have a blob created by a null principal to save, and the null
      //   principal is from the child. Null principals don't survive crossing
      //   over IPC, so there's no other principal that'll work.
      const persistable = browser.frameLoader;
      persistable.startPersistence(0, {
        onDocumentReady(doc) {
          const uri = chrome.makeURI(doc.documentURI, doc.characterSet);
          const filename = chrome.getDefaultFileName(undefined, uri, doc, null);
          chrome.internalSave(
            message.data,
            null,
            filename,
            null,
            doc.contentType,
            false /* bypass cache */,
            null /* filepicker title key */,
            null /* file chosen */,
            null /* referrer */,
            null /* initiating document */,
            false /* don't skip prompt for a location */,
            null /* cache key */,
            PrivateBrowsingUtils.isBrowserPrivate(
              browser
            ) /* private browsing ? */,
            Services.scriptSecurityManager.getSystemPrincipal()
          );
        },
        onError(status) {
          throw new Error("JSON Viewer's onSave failed in startPersistence");
        },
      });
    }
  },
};

var EXPORTED_SYMBOLS = ["DevToolsStartup"];
