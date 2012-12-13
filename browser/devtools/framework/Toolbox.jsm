/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Hosts",
                                  "resource:///modules/devtools/ToolboxHosts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommandUtils",
                                  "resource:///modules/devtools/DeveloperToolbar.jsm");

// DO NOT put Require.jsm or gcli.jsm into lazy getters as this breaks the
// requisition import a few lines down.
Cu.import("resource://gre/modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/devtools/Require.jsm");

let Requisition = require('gcli/cli').Requisition;
let CommandOutputManager = require('gcli/canon').CommandOutputManager;

this.EXPORTED_SYMBOLS = [ "Toolbox" ];

/**
 * A "Toolbox" is the component that holds all the tools for one specific
 * target. Visually, it's a document that includes the tools tabs and all
 * the iframes where the tool panels will be living in.
 *
 * @param {object} target
 *        The object the toolbox is debugging.
 * @param {Toolbox.HostType} hostType
 *        Type of host that will host the toolbox (e.g. sidebar, window)
 * @param {string} selectedTool
 *        Tool to select initially
 */
this.Toolbox = function Toolbox(target, hostType, selectedTool) {
  this._target = target;
  this._toolPanels = new Map();

  this._onLoad = this._onLoad.bind(this);
  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this.destroy = this.destroy.bind(this);

  this._target.once("close", this.destroy);

  if (!hostType) {
    hostType = Services.prefs.getCharPref(this._prefs.LAST_HOST);
  }
  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  let definitions = gDevTools.getToolDefinitions();
  if (!definitions.get(selectedTool)) {
    selectedTool = "webconsole";
  }
  this._defaultToolId = selectedTool;

  this._host = this._createHost(hostType);

  new EventEmitter(this);

  gDevTools.on("tool-registered", this._toolRegistered);
  gDevTools.on("tool-unregistered", this._toolUnregistered);
}

/**
 * The toolbox can be 'hosted' either embedded in a browser window
 * or in a separate window.
 */
Toolbox.HostType = {
  BOTTOM: "bottom",
  SIDE: "side",
  WINDOW: "window"
}

Toolbox.prototype = {
  _URL: "chrome://browser/content/devtools/framework/toolbox.xul",

  _prefs: {
    LAST_HOST: "devtools.toolbox.host",
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled"
  },

  HostType: Toolbox.HostType,

  /**
   * Returns a *copy* of the _toolPanels collection.
   *
   * @return {Map} panels
   *         All the running panels in the toolbox
   */
  getToolPanels: function TB_getToolPanels() {
    let panels = new Map();

    for (let [key, value] of this._toolPanels) {
      panels.set(key, value);
    }
    return panels;
  },

  /**
   * Get/alter the target of a Toolbox so we're debugging something different.
   * See Target.jsm for more details.
   * TODO: Do we allow |toolbox.target = null;| ?
   */
  get target() {
    return this._target;
  },

  set target(value) {
    this._target = value;
  },

  /**
   * Get/alter the host of a Toolbox, i.e. is it in browser or in a separate
   * tab. See HostType for more details.
   */
  get hostType() {
    return this._host.type;
  },

  set hostType(value) {
    this._switchToHost(value);
  },

  /**
   * Get/alter the currently displayed tool.
   */
  get currentToolId() {
    return this._currentToolId;
  },

  set currentToolId(value) {
    this._currentToolId = value;
  },

  /**
   * Get the iframe containing the toolbox UI.
   */
  get frame() {
    return this._host.frame;
  },

  /**
   * Shortcut to the document containing the toolbox UI
   */
  get doc() {
    return this.frame.contentDocument;
  },

  /**
   * Open the toolbox
   */
  open: function TBOX_open() {
    this._host.once("ready", function(event, iframe) {
      iframe.addEventListener("DOMContentLoaded", this._onLoad, true);
      iframe.setAttribute("src", this._URL);
    }.bind(this));

    this._host.open();
  },

  /**
   * Build the buttons for changing hosts. Called every time
   * the host changes.
   */
  _buildDockButtons: function TBOX_createDockButtons() {
    let dockBox = this.doc.getElementById("toolbox-dock-buttons");

    while (dockBox.firstChild) {
      dockBox.removeChild(dockBox.firstChild);
    }

    let sideEnabled = Services.prefs.getBoolPref(this._prefs.SIDE_ENABLED);

    for each (let position in this.HostType) {
      if (position == this.hostType ||
         (!sideEnabled && position == this.HostType.SIDE)) {
        continue;
      }

      let button = this.doc.createElement("toolbarbutton");
      button.id = "toolbox-dock-" + position;
      button.className = "toolbox-dock-button";
      button.addEventListener("command", function(position) {
        this.hostType = position;
      }.bind(this, position));

      dockBox.appendChild(button);
    }
  },

  /**
   * Onload handler for the toolbox's iframe
   */
  _onLoad: function TBOX_onLoad() {
    this.frame.removeEventListener("DOMContentLoaded", this._onLoad, true);
    this.isReady = true;

    let closeButton = this.doc.getElementById("toolbox-close");
    closeButton.addEventListener("command", this.destroy, true);

    this._buildDockButtons();

    this._buildTabs();
    this._buildButtons(this.frame);

    this.selectTool(this._defaultToolId);

    this.emit("ready");
  },

  /**
   * Add tabs to the toolbox UI for registered tools
   */
  _buildTabs: function TBOX_buildTabs() {
    for (let [id, definition] of gDevTools.getToolDefinitions()) {
      this._buildTabForTool(definition);
    }
  },

  /**
   * Add buttons to the UI as specified in the devtools.window.toolbarSpec pref
   *
   * @param {iframe} frame
   *        The iframe to contain the buttons
   */
  _buildButtons: function TBOX_buildButtons(frame) {
    if (this.target.isRemote) {
      return;
    }

    let toolbarSpec = CommandUtils.getCommandbarSpec("devtools.toolbox.toolbarSpec");
    let environment = { chromeDocument: frame.ownerDocument };
    let requisition = new Requisition(environment);
    requisition.commandOutputManager = new CommandOutputManager();

    let buttons = CommandUtils.createButtons(toolbarSpec, this.doc, requisition);

    let container = this.doc.getElementById("toolbox-buttons");
    buttons.forEach(function(button) {
      container.appendChild(button);
    }.bind(this));
  },

  /**
   * Build a tab for one tool definition and add to the toolbox
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildTabForTool: function TBOX_buildTabForTool(toolDefinition) {
    const MAX_ORDINAL = 99;
    if (!toolDefinition.isTargetSupported(this._target)) {
      return;
    }

    let tabs = this.doc.getElementById("toolbox-tabs");
    let deck = this.doc.getElementById("toolbox-deck");

    let id = toolDefinition.id;

    let radio = this.doc.createElement("radio");
    radio.setAttribute("label", toolDefinition.label);
    radio.className = "toolbox-tab devtools-tab";
    radio.id = "toolbox-tab-" + id;
    radio.setAttribute("toolid", id);

    let ordinal = (typeof toolDefinition.ordinal == "number") ?
                  toolDefinition.ordinal : MAX_ORDINAL;
    radio.setAttribute("ordinal", ordinal);

    radio.addEventListener("command", function(id) {
      this.selectTool(id);
    }.bind(this, id));

    let vbox = this.doc.createElement("vbox");
    vbox.className = "toolbox-panel";
    vbox.id = "toolbox-panel-" + id;

    tabs.appendChild(radio);
    deck.appendChild(vbox);
  },

  /**
   * Switch to the tool with the given id
   *
   * @param {string} id
   *        The id of the tool to switch to
   */
  selectTool: function TBOX_selectTool(id) {
    if (!this.isReady) {
      throw new Error("Can't select tool, wait for toolbox 'ready' event");
    }
    let tab = this.doc.getElementById("toolbox-tab-" + id);

    if (!tab) {
      throw new Error("No tool found");
    }

    let tabstrip = this.doc.getElementById("toolbox-tabs");

    // select the right tab
    let index = -1;
    let tabs = tabstrip.childNodes;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i] === tab) {
        index = i;
        break;
      }
    }
    tabstrip.selectedIndex = index;

    // and select the right iframe
    let deck = this.doc.getElementById("toolbox-deck");
    deck.selectedIndex = index;

    let definition = gDevTools.getToolDefinitions().get(id);

    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);
    if (!iframe) {
      iframe = this.doc.createElement("iframe");
      iframe.className = "toolbox-panel-iframe";
      iframe.id = "toolbox-panel-iframe-" + id;
      iframe.setAttribute("flex", 1);

      let vbox = this.doc.getElementById("toolbox-panel-" + id);
      vbox.appendChild(iframe);

      let boundLoad = function() {
        iframe.removeEventListener("DOMContentLoaded", boundLoad, true);
        let panel = definition.build(iframe.contentWindow, this);
        this._toolPanels.set(id, panel);

        let panelReady = function() {
          this.emit(id + "-ready", panel);
          this.emit("select", id);
          this.emit(id + "-selected", panel);
          gDevTools.emit(id + "-ready", this, panel);
        }.bind(this);

        if (panel.isReady) {
          panelReady();
        } else {
          panel.once("ready", panelReady);
        }
      }.bind(this);

      iframe.addEventListener("DOMContentLoaded", boundLoad, true);
      iframe.setAttribute("src", definition.url);
    } else {
      let panel = this._toolPanels.get(id);
      // only emit 'select' event if the iframe has been loaded
      if (panel) {
        this.emit("select", id);
        this.emit(id + "-selected", panel);
      }
    }

    Services.prefs.setCharPref(this._prefs.LAST_TOOL, id);

    this._currentToolId = id;
  },

  /**
   * Create a host object based on the given host type.
   *
   * @param {string} hostType
   *        The host type of the new host object
   *
   * @return {Host} host
   *        The created host object
   */
  _createHost: function TBOX_createHost(hostType) {
    let hostTab = this._getHostTab();
    if (!Hosts[hostType]) {
      throw new Error('Unknown hostType: '+ hostType);
    }
    let newHost = new Hosts[hostType](hostTab);

    // clean up the toolbox if its window is closed
    newHost.on("window-closed", this.destroy);

    return newHost;
  },

  /**
   * Switch to a new host for the toolbox UI. E.g.
   * bottom, sidebar, separate window.
   *
   * @param {string} hostType
   *        The host type of the new host object
   */
  _switchToHost: function TBOX_switchToHost(hostType) {
    if (hostType == this._host.type) {
      return;
    }

    let newHost = this._createHost(hostType);

    newHost.once("ready", function(event, iframe) {
      // change toolbox document's parent to the new host
      iframe.QueryInterface(Components.interfaces.nsIFrameLoaderOwner);
      iframe.swapFrameLoaders(this.frame);

      this._host.off("window-closed", this.destroy);
      this._host.destroy();

      this._host = newHost;

      Services.prefs.setCharPref(this._prefs.LAST_HOST, this._host.type);

      this._buildDockButtons();

      this.emit("host-changed");
    }.bind(this));

    newHost.open();
  },

  /**
   * Get the most appropriate host tab, either the target or the current tab
   */
  _getHostTab: function TBOX_getHostTab() {
    if (!this._target.isRemote && !this._target.isChrome) {
      return this._target.tab;
    } else {
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      return win.gBrowser.selectedTab;
    }
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} event
   *         Name of the event ("tool-registered")
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function TBOX_toolRegistered(event, toolId) {
    let defs = gDevTools.getToolDefinitions();
    let tool = defs.get(toolId);

    this._buildTabForTool(tool);
  },

  /**
   * Handler for the tool-unregistered event.
   * @param  {string} event
   *         Name of the event ("tool-unregistered")
   * @param  {string} toolId
   *         Id of the tool that was unregistered
   */
  _toolUnregistered: function TBOX_toolUnregistered(event, toolId) {
    let radio = this.doc.getElementById("toolbox-tab-" + toolId);
    let panel = this.doc.getElementById("toolbox-panel-" + toolId);

    if (this._currentToolId == toolId) {
      let nextToolName = null;
      if (radio.nextSibling) {
        nextToolName = radio.nextSibling.getAttribute("toolid");
      }
      if (radio.previousSibling) {
        nextToolName = radio.previousSibling.getAttribute("toolid");
      }
      if (nextToolName) {
        this.selectTool(nextToolName);
      }
    }

    if (radio) {
      radio.parentNode.removeChild(radio);
    }

    if (panel) {
      panel.parentNode.removeChild(panel);
    }

    if (this._toolPanels.has(toolId)) {
      let instance = this._toolPanels.get(toolId);
      instance.destroy();
      this._toolPanels.delete(toolId);
    }
  },


  /**
   * Get the toolbox's notification box
   *
   * @return The notification box element.
   */
  getNotificationBox: function TBOX_getNotificationBox() {
    return this.doc.getElementById("toolbox-notificationbox");
  },

  /**
   * Remove all UI elements, detach from target and clear up
   */
  destroy: function TBOX_destroy() {
    if (this._destroyed) {
      return;
    }

    // Remote targets need to be notified that the toolbox is being torn down.
    if (this._target && this._target.isRemote) {
      this._target.destroy();
    }
    this._target = null;

    for (let [id, panel] of this._toolPanels) {
      panel.destroy();
    }

    this._host.destroy();

    gDevTools.off("tool-registered", this._toolRegistered);
    gDevTools.off("tool-unregistered", this._toolUnregistered);

    this._destroyed = true;
    this.emit("destroyed");
  }
};
