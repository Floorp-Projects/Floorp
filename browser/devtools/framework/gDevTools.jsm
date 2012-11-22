/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "gDevTools", "DevTools", "DevToolsXULCommands" ];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");
Cu.import("resource:///modules/devtools/ToolDefinitions.jsm");
Cu.import("resource:///modules/devtools/Toolbox.jsm");
Cu.import("resource:///modules/devtools/Target.jsm");

const FORBIDDEN_IDS = new Set("toolbox", "");

/**
 * DevTools is a class that represents a set of developer tools, it holds a
 * set of tools and keeps track of open toolboxes in the browser.
 */
this.DevTools = function DevTools() {
  this._tools = new Map();
  this._toolboxes = new Map();

  // destroy() is an observer's handler so we need to preserve context.
  this.destroy = this.destroy.bind(this);

  this._trackedBrowserWindows = new Set();

  // Bind _updateMenuCheckbox() to preserve context.
  this._updateMenuCheckbox = this._updateMenuCheckbox.bind(this);

  new EventEmitter(this);

  Services.obs.addObserver(this.destroy, "quit-application", false);

  /**
   * Register the set of default tools
   */
  for (let definition of defaultTools) {
    this.registerTool(definition);
  }
}

DevTools.prototype = {
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
   * - killswitch: Property name to allow us to turn this tool on/off globally
   *               (string|required) (TODO: default to devtools.{id}.enabled?)
   * - icon: URL pointing to a graphic which will be used as the src for an
   *         16x16 img tag (string|required)
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

    toolDefinition.killswitch = toolDefinition.killswitch ||
      "devtools." + toolId + ".enabled";
    this._tools.set(toolId, toolDefinition);

    this._addToolToWindows(toolDefinition);

    this.emit("tool-registered", toolId);
  },

  /**
   * Removes all tools that match the given |toolId|
   * Needed so that add-ons can remove themselves when they are deactivated
   *
   * @param {string} toolId
   *        id of the tool to unregister
   */
  unregisterTool: function DT_unregisterTool(toolId) {
    this._tools.delete(toolId);

    this._removeToolFromWindows(toolId);

    this.emit("tool-unregistered", toolId);
  },

  /**
   * Allow ToolBoxes to get at the list of tools that they should populate
   * themselves with.
   *
   * @return {Map} tools
   *         A map of the the tool definitions registered in this instance
   */
  getToolDefinitions: function DT_getToolDefinitions() {
    let tools = new Map();

    for (let [key, value] of this._tools) {
      let enabled;

      try {
        enabled = Services.prefs.getBoolPref(value.killswitch);
      } catch(e) {
        enabled = true;
      }

      if (enabled) {
        tools.set(key, value);
      }
    }
    return tools;
  },

  /**
   * Create a toolbox to debug |target| using a window displayed in |hostType|
   * (optionally with |defaultToolId| opened)
   *
   * @param {Target} target
   *         The target the toolbox will debug
   * @param {Toolbox.HostType} hostType
   *        The type of host (bottom, top, side)
   * @param {string} defaultToolId
   *        The id of the initial tool to show
   *
   * @return {Toolbox} toolbox
   *        The toolbox that was opened
   */
  openToolbox: function DT_openToolbox(target, hostType, defaultToolId) {
    if (this._toolboxes.has(target)) {
      // only allow one toolbox per target
      return this._toolboxes.get(target);
    }

    let tb = new Toolbox(target, hostType, defaultToolId);

    this._toolboxes.set(target, tb);
    tb.once("destroyed", function() {
      this._toolboxes.delete(target);
      this._updateMenuCheckbox();
      this.emit("toolbox-destroyed", target);
    }.bind(this));

    tb.once("ready", function() {
      this.emit("toolbox-ready", tb);
      this._updateMenuCheckbox();
    }.bind(this));

    tb.open();

    return tb;
  },

  /**
   * Close the toolbox for a given target
   */
  closeToolbox: function DT_closeToolbox(target) {
    let toolbox = this._toolboxes.get(target);
    if (toolbox == null) {
      return;
    }
    toolbox.destroy();
  },

  /**
   * Open the toolbox for a specific target (not tab).
   * FIXME: We should probably merge this function and openToolbox
   *
   * @param  {Target} target
   *         The target that the toolbox should be debugging
   * @param  {String} toolId
   *         The id of the tool to open
   *
   * @return {Toolbox} toolbox
   *         The toolbox that has been opened
   */
  openToolboxForTab: function DT_openToolboxForTab(target, toolId) {
    let tb = this.getToolboxForTarget(target);

    if (tb) {
      tb.selectTool(toolId);
    } else {
      tb = this.openToolbox(target, null, toolId);
    }
    return tb;
  },

  /**
   * This function is for the benefit of command#Tools:DevToolbox in
   * browser/base/content/browser-sets.inc and should not be used outside
   * of there
   */
  toggleToolboxCommand: function(gBrowser, toolId=null) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    this.toggleToolboxForTarget(target, toolId);
  },

  /**
   * Toggle a toolbox for the given target.
   *
   * @param  {Target} target
   *         The target the toolbox is debugging
   * @param  {string} toolId
   *         The id of the tool to show in the toolbox, if it's to be opened.
   */
  toggleToolboxForTarget: function DT_toggleToolboxForTarget(target, toolId) {
    let tb = this.getToolboxForTarget(target);

    if (tb /* FIXME: && tool is showing */ ) {
      tb.destroy();
    } else {
      this.openToolboxForTab(target, toolId);
    }
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
  getToolboxForTarget: function DT_getToolboxForTarget(target) {
    return this._toolboxes.get(target);
  },

  /**
   * Return a tool panel for a given tool and target.
   *
   * @param  {String} toolId
   *         The id of the tool to open.
   * @param  {object} target
   *         The toolbox's target.
   *
   * @return {ToolPanel} panel
   *         Panel for the tool with the toolid
   */
  getPanelForTarget: function DT_getPanelForTarget(toolId, target) {
    let toolbox = this.getToolboxForTarget(target);
    if (!toolbox) {
      return undefined;
    }
    return toolbox.getToolPanels().get(toolId);
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param  {XULDocument} doc
   *         The document to which menuitems and handlers are to be added
   */
  registerBrowserWindow: function DT_registerBrowserWindow(win) {
    this._trackedBrowserWindows.add(win);
    this._addAllToolsToMenu(win.document);

    let tabContainer = win.document.getElementById("tabbrowser-tabs")
    tabContainer.addEventListener("TabSelect", this._updateMenuCheckbox, false);
  },

  /**
   * Add the menuitem for a tool to all open browser windows.
   *
   * @param {object} toolDefinition
   *        properties of the tool to add
   */
  _addToolToWindows: function DT_addToolToWindows(toolDefinition) {
    for (let win of this._trackedBrowserWindows) {
      this._addToolToMenu(toolDefinition, win.document);
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

    for (let [key, toolDefinition] of this._tools) {
      let frags = this._addToolToMenu(toolDefinition, doc, true);

      if (!frags) {
        return;
      }

      let [cmd, key, bc, appmenuitem, menuitem] = frags;

      fragCommands.appendChild(cmd);
      if (key) {
        fragKeys.appendChild(key);
      }
      fragBroadcasters.appendChild(bc);
      fragAppMenuItems.appendChild(appmenuitem);
      fragMenuItems.appendChild(menuitem);
    }

    let mcs = doc.getElementById("mainCommandSet");
    mcs.appendChild(fragCommands);

    let mks = doc.getElementById("mainKeyset");
    mks.appendChild(fragKeys);

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
   * @param {Boolean} [noAppend]
   *        Return an array of elements instead of appending them to the
   *        document. Default is false.
   */
  _addToolToMenu: function DT_addToolToMenu(toolDefinition, doc, noAppend) {
    let id = toolDefinition.id;

    // Prevent multiple entries for the same tool.
    if (doc.getElementById("Tools:" + id)) {
      return;
    }

    let cmd = doc.createElement("command");
    cmd.id = "Tools:" + id;
    cmd.setAttribute("oncommand",
        'gDevTools.toggleToolboxCommand(gBrowser, "' + id + '");');

    let key = null;
    if (toolDefinition.key) {
      key = doc.createElement("key");
      key.id = "key_" + id;

      if (toolDefinition.key.startsWith("VK_")) {
        key.setAttribute("keycode", toolDefinition.key);
      } else {
        key.setAttribute("key", toolDefinition.key);
      }

      key.setAttribute("oncommand",
          'gDevTools.toggleToolboxCommand(gBrowser, "' + id + '");');
      key.setAttribute("modifiers", toolDefinition.modifiers);
    }

    let bc = doc.createElement("broadcaster");
    bc.id = "devtoolsMenuBroadcaster_" + id;
    bc.setAttribute("label", toolDefinition.label);
    bc.setAttribute("command", "Tools:" + id);

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

    if (noAppend) {
      return [cmd, key, bc, appmenuitem, menuitem];
    } else {
      let mcs = doc.getElementById("mainCommandSet");
      mcs.appendChild(cmd);

      if (key) {
        let mks = doc.getElementById("mainKeyset");
        mks.appendChild(key);
      }

      let mbs = doc.getElementById("mainBroadcasterSet");
      mbs.appendChild(bc);

      let amp = doc.getElementById("appmenu_webDeveloper_popup");
      if (amp) {
        let amps = doc.getElementById("appmenu_devtools_separator");
        amp.insertBefore(appmenuitem, amps);
      }

      let mp = doc.getElementById("menuWebDeveloperPopup");
      let mps = doc.getElementById("menu_devtools_separator");
      mp.insertBefore(menuitem, mps);
    }
  },

  /**
   * Update the "Toggle Toolbox" checkbox in the developer tools menu. This is
   * called when a toolbox is created or destroyed.
   */
  _updateMenuCheckbox: function DT_updateMenuCheckbox() {
    for (let win of this._trackedBrowserWindows) {

      let hasToolbox = false;
      if (TargetFactory.isKnownTab(win.gBrowser.selectedTab)) {
        let target = TargetFactory.forTab(win.gBrowser.selectedTab);
        if (this._toolboxes.has(target)) {
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
   * Remove the menuitem for a tool to all open browser windows.
   *
   * @param {object} toolId
   *        id of the tool to remove
   */
  _removeToolFromWindows: function DT_removeToolFromWindows(toolId) {
    for (let win of this._trackedBrowserWindows) {
      this._removeToolFromMenu(toolId, win.document);
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
    command.parentNode.removeChild(command);

    let key = doc.getElementById("key_" + toolId);
    if (key) {
      key.parentNode.removeChild(key);
    }

    let bc = doc.getElementById("devtoolsMenuBroadcaster_" + toolId);
    bc.parentNode.removeChild(bc);

    /*
    // FIXME: item is null in testing. This is the only place to use
    // "appmenu_devToolbar" + toolId, so it seems clear that this is wrong
    let item = doc.getElementById("appmenu_devToolbar" + toolId);
    item.parentNode.removeChild(item);
    */
  },

  /**
   * Called on browser unload to remove menu entries, toolboxes and event
   * listeners from the closed browser window.
   *
   * @param  {XULWindow} win
   *         The window containing the menu entry
   */
  forgetBrowserWindow: function DT_forgetBrowserWindow(win) {
    if (!this._tools) {
      return;
    }

    this._trackedBrowserWindows.delete(win);

    // Destroy toolboxes for closed window
    for (let [target, toolbox] of this._toolboxes) {
      if (toolbox.frame.ownerDocument.defaultView == win) {
        toolbox.destroy();
      }
    }

    let tabContainer = win.document.getElementById("tabbrowser-tabs")
    tabContainer.removeEventListener("TabSelect",
                                     this._updateMenuCheckbox, false);
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function() {
    Services.obs.removeObserver(this.destroy, "quit-application");

    delete this._trackedBrowserWindows;
    delete this._tools;
    delete this._toolboxes;
  },
};

/**
 * gDevTools is a singleton that controls the Firefox Developer Tools.
 *
 * It is an instance of a DevTools class that holds a set of tools. It has the
 * same lifetime as the browser.
 */
this.gDevTools = new DevTools();

/**
 * DevToolsXULCommands exposes methods used by browser's <command>s.
 */
this.DevToolsXULCommands = {
  openConnectScreen: function(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("chrome://browser/content/devtools/connect.xhtml");
  },
}
