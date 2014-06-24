/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "gDevTools", "DevTools", "gDevToolsBrowser" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "promise", "resource://gre/modules/Promise.jsm", "Promise");

const EventEmitter = devtools.require("devtools/toolkit/event-emitter");
const FORBIDDEN_IDS = new Set(["toolbox", ""]);
const MAX_ORDINAL = 99;


/**
 * DevTools is a class that represents a set of developer tools, it holds a
 * set of tools and keeps track of open toolboxes in the browser.
 */
this.DevTools = function DevTools() {
  this._tools = new Map();     // Map<toolId, tool>
  this._toolboxes = new Map(); // Map<target, toolbox>

  // destroy() is an observer's handler so we need to preserve context.
  this.destroy = this.destroy.bind(this);
  this._teardown = this._teardown.bind(this);

  this._testing = false;

  EventEmitter.decorate(this);

  Services.obs.addObserver(this._teardown, "devtools-unloaded", false);
  Services.obs.addObserver(this.destroy, "quit-application", false);
}

DevTools.prototype = {
  /**
   * When the testing flag is set we take appropriate action to prevent race
   * conditions in our testing environment. This means setting
   * dom.send_after_paint_to_content to false to prevent infinite MozAfterPaint
   * loops and not autohiding the highlighter.
   */
  get testing() {
    return this._testing;
  },

  set testing(state) {
    this._testing = state;

    if (state) {
      // dom.send_after_paint_to_content is set to true (non-default) in
      // testing/profiles/prefs_general.js so lets set it to the same as it is
      // in a default browser profile for the duration of the test.
      Services.prefs.setBoolPref("dom.send_after_paint_to_content", false);
    }
  },

  /**
   * Register a new developer tool.
   *
   * A definition is a light object that holds different information about a
   * developer tool. This object is not supposed to have any operational code.
   * See it as a "manifest".
   * The only actual code lives in the build() function, which will be used to
   * start an instance of this tool.
   *
   * Each toolDefinition has the following properties:
   * - id: Unique identifier for this tool (string|required)
   * - visibilityswitch: Property name to allow us to hide this tool from the
   *                     DevTools Toolbox.
   *                     A falsy value indicates that it cannot be hidden.
   * - icon: URL pointing to a graphic which will be used as the src for an
   *         16x16 img tag (string|required)
   * - invertIconForLightTheme: The icon can automatically have an inversion
   *         filter applied (default is false).  All builtin tools are true, but
   *         addons may omit this to prevent unwanted changes to the `icon`
   *         image. See browser/themes/shared/devtools/filters.svg#invert for
   *         the filter being applied to the images (boolean|optional)
   * - url: URL pointing to a XUL/XHTML document containing the user interface
   *        (string|required)
   * - label: Localized name for the tool to be displayed to the user
   *          (string|required)
   * - build: Function that takes an iframe, which has been populated with the
   *          markup from |url|, and also the toolbox containing the panel.
   *          And returns an instance of ToolPanel (function|required)
   */
  registerTool: function DT_registerTool(toolDefinition) {
    let toolId = toolDefinition.id;

    if (!toolId || FORBIDDEN_IDS.has(toolId)) {
      throw new Error("Invalid definition.id");
    }

    // Make sure that additional tools will always be able to be hidden.
    // When being called from main.js, defaultTools has not yet been exported.
    // But, we can assume that in this case, it is a default tool.
    if (devtools.defaultTools && devtools.defaultTools.indexOf(toolDefinition) == -1) {
      toolDefinition.visibilityswitch = "devtools." + toolId + ".enabled";
    }

    this._tools.set(toolId, toolDefinition);

    this.emit("tool-registered", toolId);
  },

  /**
   * Removes all tools that match the given |toolId|
   * Needed so that add-ons can remove themselves when they are deactivated
   *
   * @param {string|object} tool
   *        Definition or the id of the tool to unregister. Passing the
   *        tool id should be avoided as it is a temporary measure.
   * @param {boolean} isQuitApplication
   *        true to indicate that the call is due to app quit, so we should not
   *        cause a cascade of costly events
   */
  unregisterTool: function DT_unregisterTool(tool, isQuitApplication) {
    let toolId = null;
    if (typeof tool == "string") {
      toolId = tool;
      tool = this._tools.get(tool);
    }
    else {
      toolId = tool.id;
    }
    this._tools.delete(toolId);

    if (!isQuitApplication) {
      this.emit("tool-unregistered", tool);
    }
  },

  /**
   * Sorting function used for sorting tools based on their ordinals.
   */
  ordinalSort: function DT_ordinalSort(d1, d2) {
    let o1 = (typeof d1.ordinal == "number") ? d1.ordinal : MAX_ORDINAL;
    let o2 = (typeof d2.ordinal == "number") ? d2.ordinal : MAX_ORDINAL;
    return o1 - o2;
  },

  getDefaultTools: function DT_getDefaultTools() {
    return devtools.defaultTools.sort(this.ordinalSort);
  },

  getAdditionalTools: function DT_getAdditionalTools() {
    let tools = [];
    for (let [key, value] of this._tools) {
      if (devtools.defaultTools.indexOf(value) == -1) {
        tools.push(value);
      }
    }
    return tools.sort(this.ordinalSort);
  },

  /**
   * Get a tool definition if it exists and is enabled.
   *
   * @param {string} toolId
   *        The id of the tool to show
   *
   * @return {ToolDefinition|null} tool
   *         The ToolDefinition for the id or null.
   */
  getToolDefinition: function DT_getToolDefinition(toolId) {
    let tool = this._tools.get(toolId);
    if (!tool) {
      return null;
    } else if (!tool.visibilityswitch) {
      return tool;
    }

    let enabled;
    try {
      enabled = Services.prefs.getBoolPref(tool.visibilityswitch);
    } catch (e) {
      enabled = true;
    }

    return enabled ? tool : null;
  },

  /**
   * Allow ToolBoxes to get at the list of tools that they should populate
   * themselves with.
   *
   * @return {Map} tools
   *         A map of the the tool definitions registered in this instance
   */
  getToolDefinitionMap: function DT_getToolDefinitionMap() {
    let tools = new Map();

    for (let [id, definition] of this._tools) {
      if (this.getToolDefinition(id)) {
        tools.set(id, definition);
      }
    }

    return tools;
  },

  /**
   * Tools have an inherent ordering that can't be represented in a Map so
   * getToolDefinitionArray provides an alternative representation of the
   * definitions sorted by ordinal value.
   *
   * @return {Array} tools
   *         A sorted array of the tool definitions registered in this instance
   */
  getToolDefinitionArray: function DT_getToolDefinitionArray() {
    let definitions = [];

    for (let [id, definition] of this._tools) {
      if (this.getToolDefinition(id)) {
        definitions.push(definition);
      }
    }

    return definitions.sort(this.ordinalSort);
  },

  /**
   * Show a Toolbox for a target (either by creating a new one, or if a toolbox
   * already exists for the target, by bring to the front the existing one)
   * If |toolId| is specified then the displayed toolbox will have the
   * specified tool selected.
   * If |hostType| is specified then the toolbox will be displayed using the
   * specified HostType.
   *
   * @param {Target} target
   *         The target the toolbox will debug
   * @param {string} toolId
   *        The id of the tool to show
   * @param {Toolbox.HostType} hostType
   *        The type of host (bottom, window, side)
   * @param {object} hostOptions
   *        Options for host specifically
   *
   * @return {Toolbox} toolbox
   *        The toolbox that was opened
   */
  showToolbox: function(target, toolId, hostType, hostOptions) {
    let deferred = promise.defer();

    let toolbox = this._toolboxes.get(target);
    if (toolbox) {

      let hostPromise = (hostType != null && toolbox.hostType != hostType) ?
          toolbox.switchHost(hostType) :
          promise.resolve(null);

      if (toolId != null && toolbox.currentToolId != toolId) {
        hostPromise = hostPromise.then(function() {
          return toolbox.selectTool(toolId);
        });
      }

      return hostPromise.then(function() {
        toolbox.raise();
        return toolbox;
      });
    }
    else {
      // No toolbox for target, create one
      toolbox = new devtools.Toolbox(target, toolId, hostType, hostOptions);

      this._toolboxes.set(target, toolbox);

      toolbox.once("destroyed", () => {
        this._toolboxes.delete(target);
        this.emit("toolbox-destroyed", target);
      });

      // If we were asked for a specific tool then we need to wait for the
      // tool to be ready, otherwise we can just wait for toolbox open
      if (toolId != null) {
        toolbox.once(toolId + "-ready", (event, panel) => {
          this.emit("toolbox-ready", toolbox);
          deferred.resolve(toolbox);
        });
        toolbox.open();
      }
      else {
        toolbox.open().then(() => {
          deferred.resolve(toolbox);
          this.emit("toolbox-ready", toolbox);
        });
      }
    }

    return deferred.promise;
  },

  /**
   * Return the toolbox for a given target.
   *
   * @param  {object} target
   *         Target value e.g. the target that owns this toolbox
   *
   * @return {Toolbox} toolbox
   *         The toobox that is debugging the given target
   */
  getToolbox: function DT_getToolbox(target) {
    return this._toolboxes.get(target);
  },

  /**
   * Close the toolbox for a given target
   *
   * @return promise
   *         This promise will resolve to false if no toolbox was found
   *         associated to the target. true, if the toolbox was successfuly
   *         closed.
   */
  closeToolbox: function DT_closeToolbox(target) {
    let toolbox = this._toolboxes.get(target);
    if (toolbox == null) {
      return promise.resolve(false);
    }
    return toolbox.destroy().then(() => true);
  },

  /**
   * Called to tear down a tools provider.
   */
  _teardown: function DT_teardown() {
    for (let [target, toolbox] of this._toolboxes) {
      toolbox.destroy();
    }
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function() {
    Services.obs.removeObserver(this.destroy, "quit-application");
    Services.obs.removeObserver(this._teardown, "devtools-unloaded");

    for (let [key, tool] of this.getToolDefinitionMap()) {
      this.unregisterTool(key, true);
    }

    // Cleaning down the toolboxes: i.e.
    //   for (let [target, toolbox] of this._toolboxes) toolbox.destroy();
    // Is taken care of by the gDevToolsBrowser.forgetBrowserWindow
  },

  /**
   * Iterator that yields each of the toolboxes.
   */
  '@@iterator': function*() {
    for (let toolbox of this._toolboxes) {
      yield toolbox;
    }
  }
};

/**
 * gDevTools is a singleton that controls the Firefox Developer Tools.
 *
 * It is an instance of a DevTools class that holds a set of tools. It has the
 * same lifetime as the browser.
 */
let gDevTools = new DevTools();
this.gDevTools = gDevTools;

/**
 * gDevToolsBrowser exposes functions to connect the gDevTools instance with a
 * Firefox instance.
 */
let gDevToolsBrowser = {
  /**
   * A record of the windows whose menus we altered, so we can undo the changes
   * as the window is closed
   */
  _trackedBrowserWindows: new Set(),

  /**
   * This function is for the benefit of Tools:DevToolbox in
   * browser/base/content/browser-sets.inc and should not be used outside
   * of there
   */
  toggleToolboxCommand: function(gBrowser) {
    let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    toolbox ? toolbox.destroy() : gDevTools.showToolbox(target);
  },

  toggleBrowserToolboxCommand: function(gBrowser) {
    let target = devtools.TargetFactory.forWindow(gBrowser.ownerDocument.defaultView);
    let toolbox = gDevTools.getToolbox(target);

    toolbox ? toolbox.destroy()
     : gDevTools.showToolbox(target, "inspector", Toolbox.HostType.WINDOW);
  },

  /**
   * This function ensures the right commands are enabled in a window,
   * depending on their relevant prefs. It gets run when a window is registered,
   * or when any of the devtools prefs change.
   */
  updateCommandAvailability: function(win) {
    let doc = win.document;

    function toggleCmd(id, isEnabled) {
      let cmd = doc.getElementById(id);
      if (isEnabled) {
        cmd.removeAttribute("disabled");
        cmd.removeAttribute("hidden");
      } else {
        cmd.setAttribute("disabled", "true");
        cmd.setAttribute("hidden", "true");
      }
    };

    // Enable developer toolbar?
    let devToolbarEnabled = Services.prefs.getBoolPref("devtools.toolbar.enabled");
    toggleCmd("Tools:DevToolbar", devToolbarEnabled);
    let focusEl = doc.getElementById("Tools:DevToolbarFocus");
    if (devToolbarEnabled) {
      focusEl.removeAttribute("disabled");
    } else {
      focusEl.setAttribute("disabled", "true");
    }
    if (devToolbarEnabled && Services.prefs.getBoolPref("devtools.toolbar.visible")) {
      win.DeveloperToolbar.show(false);
    }

    // Enable WebIDE?
    let webIDEEnabled = Services.prefs.getBoolPref("devtools.webide.enabled");
    toggleCmd("Tools:WebIDE", webIDEEnabled);

    // Enable App Manager?
    let appMgrEnabled = Services.prefs.getBoolPref("devtools.appmanager.enabled");
    toggleCmd("Tools:DevAppMgr", !webIDEEnabled && appMgrEnabled);

    // Enable Browser Toolbox?
    let chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    let devtoolsRemoteEnabled = Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
    let remoteEnabled = chromeEnabled && devtoolsRemoteEnabled &&
                        Services.prefs.getBoolPref("devtools.debugger.chrome-enabled");
    toggleCmd("Tools:BrowserToolbox", remoteEnabled);

    // Enable Error Console?
    let consoleEnabled = Services.prefs.getBoolPref("devtools.errorconsole.enabled");
    toggleCmd("Tools:ErrorConsole", consoleEnabled);

    // Enable DevTools connection screen, if the preference allows this.
    toggleCmd("Tools:DevToolsConnect", devtoolsRemoteEnabled);
  },

  observe: function(subject, topic, prefName) {
    if (prefName.endsWith("enabled")) {
      for (let win of this._trackedBrowserWindows) {
        this.updateCommandAvailability(win);
      }
    }
  },

  _prefObserverRegistered: false,

  ensurePrefObserver: function() {
    if (!this._prefObserverRegistered) {
      this._prefObserverRegistered = true;
      Services.prefs.addObserver("devtools.", this, false);
    }
  },


  /**
   * This function is for the benefit of Tools:{toolId} commands,
   * triggered from the WebDeveloper menu and keyboard shortcuts.
   *
   * selectToolCommand's behavior:
   * - if the toolbox is closed,
   *   we open the toolbox and select the tool
   * - if the toolbox is open, and the targetted tool is not selected,
   *   we select it
   * - if the toolbox is open, and the targetted tool is selected,
   *   and the host is NOT a window, we close the toolbox
   * - if the toolbox is open, and the targetted tool is selected,
   *   and the host is a window, we raise the toolbox window
   */
  selectToolCommand: function(gBrowser, toolId) {
    let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolbox &&
        (toolbox.currentToolId == toolId ||
          (toolId == "webconsole" && toolbox.splitConsole)))
    {
      toolbox.fireCustomKey(toolId);

      if (toolDefinition.preventClosingOnKey || toolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
        toolbox.raise();
      } else {
        toolbox.destroy();
      }
      gDevTools.emit("select-tool-command", toolId);
    } else {
      gDevTools.showToolbox(target, toolId).then(() => {
        let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
        let toolbox = gDevTools.getToolbox(target);

        toolbox.fireCustomKey(toolId);
        gDevTools.emit("select-tool-command", toolId);
      });
    }
  },

  /**
   * Open a tab to allow connects to a remote browser
   */
  openConnectScreen: function(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("chrome://browser/content/devtools/connect.xhtml");
  },

  /**
   * Open the App Manager
   */
  openAppManager: function(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("about:app-manager");
  },

  /**
   * Open WebIDE
   */
  openWebIDE: function() {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].getService(Ci.nsIWindowWatcher);
      ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);
    }
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param {XULDocument} doc
   *        The document to which menuitems and handlers are to be added
   */
  registerBrowserWindow: function DT_registerBrowserWindow(win) {
    this.updateCommandAvailability(win);
    this.ensurePrefObserver();
    gDevToolsBrowser._trackedBrowserWindows.add(win);
    gDevToolsBrowser._addAllToolsToMenu(win.document);

    if (this._isFirebugInstalled()) {
      let broadcaster = win.document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
      broadcaster.removeAttribute("key");
    }

    let tabContainer = win.document.getElementById("tabbrowser-tabs")
    tabContainer.addEventListener("TabSelect",
                                  gDevToolsBrowser._updateMenuCheckbox, false);
  },

  /**
   * Add a <key> to <keyset id="devtoolsKeyset">.
   * Appending a <key> element is not always enough. The <keyset> needs
   * to be detached and reattached to make sure the <key> is taken into
   * account (see bug 832984).
   *
   * @param {XULDocument} doc
   *        The document to which keys are to be added
   * @param {XULElement} or {DocumentFragment} keys
   *        Keys to add
   */
  attachKeybindingsToBrowser: function DT_attachKeybindingsToBrowser(doc, keys) {
    let devtoolsKeyset = doc.getElementById("devtoolsKeyset");

    if (!devtoolsKeyset) {
      devtoolsKeyset = doc.createElement("keyset");
      devtoolsKeyset.setAttribute("id", "devtoolsKeyset");
    }
    devtoolsKeyset.appendChild(keys);
    let mainKeyset = doc.getElementById("mainKeyset");
    mainKeyset.parentNode.insertBefore(devtoolsKeyset, mainKeyset);
  },

  /**
   * Hook the JS debugger tool to the "Debug Script" button of the slow script
   * dialog.
   */
  setSlowScriptDebugHandler: function DT_setSlowScriptDebugHandler() {
    let debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

    debugService.activationHandler = function(aWindow) {
      let chromeWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIDocShellTreeItem)
                                .rootTreeItem
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow)
                                .QueryInterface(Ci.nsIDOMChromeWindow);
      let target = devtools.TargetFactory.forTab(chromeWindow.gBrowser.selectedTab);

      let setupFinished = false;
      gDevTools.showToolbox(target, "jsdebugger").then(toolbox => {
        let threadClient = toolbox.getCurrentPanel().panelWin.gThreadClient;

        // Break in place, which means resuming the debuggee thread and pausing
        // right before the next step happens.
        switch (threadClient.state) {
          case "paused":
            // When the debugger is already paused.
            threadClient.breakOnNext();
            setupFinished = true;
            break;
          case "attached":
            // When the debugger is already open.
            threadClient.interrupt(() => {
              threadClient.breakOnNext();
              setupFinished = true;
            });
            break;
          case "resuming":
            // The debugger is newly opened.
            threadClient.addOneTimeListener("resumed", () => {
              threadClient.interrupt(() => {
                threadClient.breakOnNext();
                setupFinished = true;
              });
            });
            break;
          default:
            throw Error("invalid thread client state in slow script debug handler: " +
                        threadClient.state);
          }
      });

      // Don't return from the interrupt handler until the debugger is brought
      // up; no reason to continue executing the slow script.
      let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.enterModalState();
      while (!setupFinished) {
        tm.currentThread.processNextEvent(true);
      }
      utils.leaveModalState();
    };
  },

  /**
   * Unset the slow script debug handler.
   */
  unsetSlowScriptDebugHandler: function DT_unsetSlowScriptDebugHandler() {
    let debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    debugService.activationHandler = undefined;
  },

  /**
   * Detect the presence of a Firebug.
   *
   * @return promise
   */
  _isFirebugInstalled: function DT_isFirebugInstalled() {
    let bootstrappedAddons = Services.prefs.getCharPref("extensions.bootstrappedAddons");
    return bootstrappedAddons.indexOf("firebug@software.joehewitt.com") != -1;
  },

  /**
   * Add the menuitem for a tool to all open browser windows.
   *
   * @param {object} toolDefinition
   *        properties of the tool to add
   */
  _addToolToWindows: function DT_addToolToWindows(toolDefinition) {
    // No menu item or global shortcut is required for options panel.
    if (!toolDefinition.inMenu) {
      return;
    }

    // Skip if the tool is disabled.
    try {
      if (toolDefinition.visibilityswitch &&
         !Services.prefs.getBoolPref(toolDefinition.visibilityswitch)) {
        return;
      }
    } catch(e) {}

    // We need to insert the new tool in the right place, which means knowing
    // the tool that comes before the tool that we're trying to add
    let allDefs = gDevTools.getToolDefinitionArray();
    let prevDef;
    for (let def of allDefs) {
      if (!def.inMenu) {
        continue;
      }
      if (def === toolDefinition) {
        break;
      }
      prevDef = def;
    }

    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      let doc = win.document;
      let elements = gDevToolsBrowser._createToolMenuElements(toolDefinition, doc);

      doc.getElementById("mainCommandSet").appendChild(elements.cmd);

      if (elements.key) {
        this.attachKeybindingsToBrowser(doc, elements.key);
      }

      doc.getElementById("mainBroadcasterSet").appendChild(elements.bc);

      let amp = doc.getElementById("appmenu_webDeveloper_popup");
      if (amp) {
        let ref;

        if (prevDef != null) {
          let menuitem = doc.getElementById("appmenuitem_" + prevDef.id);
          ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
        } else {
          ref = doc.getElementById("appmenu_devtools_separator");
        }

        if (ref) {
          amp.insertBefore(elements.appmenuitem, ref);
        }
      }

      let mp = doc.getElementById("menuWebDeveloperPopup");
      if (mp) {
        let ref;

        if (prevDef != null) {
          let menuitem = doc.getElementById("menuitem_" + prevDef.id);
          ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
        } else {
          ref = doc.getElementById("menu_devtools_separator");
        }

        if (ref) {
          mp.insertBefore(elements.menuitem, ref);
        }
      }
    }

    if (toolDefinition.id === "jsdebugger") {
      gDevToolsBrowser.setSlowScriptDebugHandler();
    }
  },

  /**
   * Add all tools to the developer tools menu of a window.
   *
   * @param {XULDocument} doc
   *        The document to which the tool items are to be added.
   */
  _addAllToolsToMenu: function DT_addAllToolsToMenu(doc) {
    let fragCommands = doc.createDocumentFragment();
    let fragKeys = doc.createDocumentFragment();
    let fragBroadcasters = doc.createDocumentFragment();
    let fragAppMenuItems = doc.createDocumentFragment();
    let fragMenuItems = doc.createDocumentFragment();

    for (let toolDefinition of gDevTools.getToolDefinitionArray()) {
      if (!toolDefinition.inMenu) {
        continue;
      }

      let elements = gDevToolsBrowser._createToolMenuElements(toolDefinition, doc);

      if (!elements) {
        return;
      }

      fragCommands.appendChild(elements.cmd);
      if (elements.key) {
        fragKeys.appendChild(elements.key);
      }
      fragBroadcasters.appendChild(elements.bc);
      fragAppMenuItems.appendChild(elements.appmenuitem);
      fragMenuItems.appendChild(elements.menuitem);
    }

    let mcs = doc.getElementById("mainCommandSet");
    mcs.appendChild(fragCommands);

    this.attachKeybindingsToBrowser(doc, fragKeys);

    let mbs = doc.getElementById("mainBroadcasterSet");
    mbs.appendChild(fragBroadcasters);

    let amp = doc.getElementById("appmenu_webDeveloper_popup");
    if (amp) {
      let amps = doc.getElementById("appmenu_devtools_separator");
      amp.insertBefore(fragAppMenuItems, amps);
    }

    let mp = doc.getElementById("menuWebDeveloperPopup");
    let mps = doc.getElementById("menu_devtools_separator");
    mp.insertBefore(fragMenuItems, mps);
  },

  /**
   * Add a menu entry for a tool definition
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to add a menu entry.
   * @param {XULDocument} doc
   *        The document to which the tool menu item is to be added.
   */
  _createToolMenuElements: function DT_createToolMenuElements(toolDefinition, doc) {
    let id = toolDefinition.id;

    // Prevent multiple entries for the same tool.
    if (doc.getElementById("Tools:" + id)) {
      return;
    }

    let cmd = doc.createElement("command");
    cmd.id = "Tools:" + id;
    cmd.setAttribute("oncommand",
        'gDevToolsBrowser.selectToolCommand(gBrowser, "' + id + '");');

    let key = null;
    if (toolDefinition.key) {
      key = doc.createElement("key");
      key.id = "key_" + id;

      if (toolDefinition.key.startsWith("VK_")) {
        key.setAttribute("keycode", toolDefinition.key);
      } else {
        key.setAttribute("key", toolDefinition.key);
      }

      key.setAttribute("command", cmd.id);
      key.setAttribute("modifiers", toolDefinition.modifiers);
    }

    let bc = doc.createElement("broadcaster");
    bc.id = "devtoolsMenuBroadcaster_" + id;
    bc.setAttribute("label", toolDefinition.menuLabel || toolDefinition.label);
    bc.setAttribute("command", cmd.id);

    if (key) {
      bc.setAttribute("key", "key_" + id);
    }

    let appmenuitem = doc.createElement("menuitem");
    appmenuitem.id = "appmenuitem_" + id;
    appmenuitem.setAttribute("observes", "devtoolsMenuBroadcaster_" + id);

    let menuitem = doc.createElement("menuitem");
    menuitem.id = "menuitem_" + id;
    menuitem.setAttribute("observes", "devtoolsMenuBroadcaster_" + id);

    if (toolDefinition.accesskey) {
      menuitem.setAttribute("accesskey", toolDefinition.accesskey);
    }

    return {
      cmd: cmd,
      key: key,
      bc: bc,
      appmenuitem: appmenuitem,
      menuitem: menuitem
    };
  },

  /**
   * Update the "Toggle Tools" checkbox in the developer tools menu. This is
   * called when a toolbox is created or destroyed.
   */
  _updateMenuCheckbox: function DT_updateMenuCheckbox() {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {

      let hasToolbox = false;
      if (devtools.TargetFactory.isKnownTab(win.gBrowser.selectedTab)) {
        let target = devtools.TargetFactory.forTab(win.gBrowser.selectedTab);
        if (gDevTools._toolboxes.has(target)) {
          hasToolbox = true;
        }
      }

      let broadcaster = win.document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
      if (hasToolbox) {
        broadcaster.setAttribute("checked", "true");
      } else {
        broadcaster.removeAttribute("checked");
      }
    }
  },

  /**
   * Connects to the SPS profiler when the developer tools are open.
   */
  _connectToProfiler: function DT_connectToProfiler() {
    let ProfilerController = devtools.require("devtools/profiler/controller");

    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      if (devtools.TargetFactory.isKnownTab(win.gBrowser.selectedTab)) {
        let target = devtools.TargetFactory.forTab(win.gBrowser.selectedTab);
        if (gDevTools._toolboxes.has(target)) {
          target.makeRemote().then(() => {
            let profiler = new ProfilerController(target);
            profiler.connect();
          }).then(null, Cu.reportError);

          return;
        }
      }
    }
  },

  /**
   * Remove the menuitem for a tool to all open browser windows.
   *
   * @param {string} toolId
   *        id of the tool to remove
   */
  _removeToolFromWindows: function DT_removeToolFromWindows(toolId) {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      gDevToolsBrowser._removeToolFromMenu(toolId, win.document);
    }

    if (toolId === "jsdebugger") {
      gDevToolsBrowser.unsetSlowScriptDebugHandler();
    }
  },

  /**
   * Remove a tool's menuitem from a window
   *
   * @param {string} toolId
   *        Id of the tool to add a menu entry for
   * @param {XULDocument} doc
   *        The document to which the tool menu item is to be removed from
   */
  _removeToolFromMenu: function DT_removeToolFromMenu(toolId, doc) {
    let command = doc.getElementById("Tools:" + toolId);
    if (command) {
      command.parentNode.removeChild(command);
    }

    let key = doc.getElementById("key_" + toolId);
    if (key) {
      key.parentNode.removeChild(key);
    }

    let bc = doc.getElementById("devtoolsMenuBroadcaster_" + toolId);
    if (bc) {
      bc.parentNode.removeChild(bc);
    }

    let appmenuitem = doc.getElementById("appmenuitem_" + toolId);
    if (appmenuitem) {
      appmenuitem.parentNode.removeChild(appmenuitem);
    }

    let menuitem = doc.getElementById("menuitem_" + toolId);
    if (menuitem) {
      menuitem.parentNode.removeChild(menuitem);
    }
  },

  /**
   * Called on browser unload to remove menu entries, toolboxes and event
   * listeners from the closed browser window.
   *
   * @param  {XULWindow} win
   *         The window containing the menu entry
   */
  forgetBrowserWindow: function DT_forgetBrowserWindow(win) {
    gDevToolsBrowser._trackedBrowserWindows.delete(win);

    // Destroy toolboxes for closed window
    for (let [target, toolbox] of gDevTools._toolboxes) {
      if (toolbox.frame && toolbox.frame.ownerDocument.defaultView == win) {
        toolbox.destroy();
      }
    }

    let tabContainer = win.document.getElementById("tabbrowser-tabs")
    tabContainer.removeEventListener("TabSelect",
                                     gDevToolsBrowser._updateMenuCheckbox, false);
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function() {
    gDevTools.off("toolbox-ready", gDevToolsBrowser._connectToProfiler);
    Services.prefs.removeObserver("devtools.", gDevToolsBrowser);
    Services.obs.removeObserver(gDevToolsBrowser.destroy, "quit-application");
  },
}

this.gDevToolsBrowser = gDevToolsBrowser;

gDevTools.on("tool-registered", function(ev, toolId) {
  let toolDefinition = gDevTools._tools.get(toolId);
  gDevToolsBrowser._addToolToWindows(toolDefinition);
});

gDevTools.on("tool-unregistered", function(ev, toolId) {
  if (typeof toolId != "string") {
    toolId = toolId.id;
  }
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenuCheckbox);
gDevTools.on("toolbox-ready", gDevToolsBrowser._connectToProfiler);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenuCheckbox);

Services.obs.addObserver(gDevToolsBrowser.destroy, "quit-application", false);

// Load the browser devtools main module as the loader's main module.
devtools.main("main");
