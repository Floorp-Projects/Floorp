/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MAX_ORDINAL = 99;
const ZOOM_PREF = "devtools.toolbox.zoomValue";
const MIN_ZOOM = 0.5;
const MAX_ZOOM = 2;

let {Cc, Ci, Cu} = require("chrome");
let promise = require("sdk/core/promise");
let EventEmitter = require("devtools/shared/event-emitter");
let Telemetry = require("devtools/shared/telemetry");
let HUDService = require("devtools/webconsole/hudservice");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource:///modules/devtools/scratchpad-manager.jsm");
Cu.import("resource:///modules/devtools/DOMHelpers.jsm");
Cu.import("resource://gre/modules/Task.jsm");

loader.lazyGetter(this, "Hosts", () => require("devtools/framework/toolbox-hosts").Hosts);

loader.lazyImporter(this, "CommandUtils", "resource:///modules/devtools/DeveloperToolbar.jsm");

loader.lazyGetter(this, "toolboxStrings", () => {
  let bundle = Services.strings.createBundle("chrome://browser/locale/devtools/toolbox.properties");
  return (name, ...args) => {
    try {
      if (!args.length) {
        return bundle.GetStringFromName(name);
      }
      return bundle.formatStringFromName(name, args, args.length);
    } catch (ex) {
      Services.console.logStringMessage("Error reading '" + name + "'");
      return null;
    }
  };
});

loader.lazyGetter(this, "Requisition", () => {
  let {require} = Cu.import("resource://gre/modules/devtools/Require.jsm", {});
  Cu.import("resource://gre/modules/devtools/gcli.jsm", {});
  return require("gcli/cli").Requisition;
});

loader.lazyGetter(this, "Selection", () => require("devtools/framework/selection").Selection);
loader.lazyGetter(this, "InspectorFront", () => require("devtools/server/actors/inspector").InspectorFront);

/**
 * A "Toolbox" is the component that holds all the tools for one specific
 * target. Visually, it's a document that includes the tools tabs and all
 * the iframes where the tool panels will be living in.
 *
 * @param {object} target
 *        The object the toolbox is debugging.
 * @param {string} selectedTool
 *        Tool to select initially
 * @param {Toolbox.HostType} hostType
 *        Type of host that will host the toolbox (e.g. sidebar, window)
 * @param {object} hostOptions
 *        Options for host specifically
 */
function Toolbox(target, selectedTool, hostType, hostOptions) {
  this._target = target;
  this._toolPanels = new Map();
  this._telemetry = new Telemetry();

  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this._refreshHostTitle = this._refreshHostTitle.bind(this);
  this._splitConsoleOnKeypress = this._splitConsoleOnKeypress.bind(this)
  this.destroy = this.destroy.bind(this);
  this.highlighterUtils = new ToolboxHighlighterUtils(this);

  this._target.on("close", this.destroy);

  if (!hostType) {
    hostType = Services.prefs.getCharPref(this._prefs.LAST_HOST);
  }
  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  if (!gDevTools.getToolDefinition(selectedTool)) {
    selectedTool = "webconsole";
  }
  this._defaultToolId = selectedTool;

  this._host = this._createHost(hostType, hostOptions);

  EventEmitter.decorate(this);

  this._target.on("navigate", this._refreshHostTitle);
  this.on("host-changed", this._refreshHostTitle);
  this.on("select", this._refreshHostTitle);

  gDevTools.on("tool-registered", this._toolRegistered);
  gDevTools.on("tool-unregistered", this._toolUnregistered);
}
exports.Toolbox = Toolbox;

/**
 * The toolbox can be 'hosted' either embedded in a browser window
 * or in a separate window.
 */
Toolbox.HostType = {
  BOTTOM: "bottom",
  SIDE: "side",
  WINDOW: "window",
  CUSTOM: "custom"
};

Toolbox.prototype = {
  _URL: "chrome://browser/content/devtools/framework/toolbox.xul",

  _prefs: {
    LAST_HOST: "devtools.toolbox.host",
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled"
  },

  currentToolId: null,

  /**
   * Returns a *copy* of the _toolPanels collection.
   *
   * @return {Map} panels
   *         All the running panels in the toolbox
   */
  getToolPanels: function() {
    return new Map(this._toolPanels);
  },

  /**
   * Access the panel for a given tool
   */
  getPanel: function(id) {
    return this._toolPanels.get(id);
  },

  /**
   * This is a shortcut for getPanel(currentToolId) because it is much more
   * likely that we're going to want to get the panel that we've just made
   * visible
   */
  getCurrentPanel: function() {
    return this._toolPanels.get(this.currentToolId);
  },

  /**
   * Get/alter the target of a Toolbox so we're debugging something different.
   * See Target.jsm for more details.
   * TODO: Do we allow |toolbox.target = null;| ?
   */
  get target() {
    return this._target;
  },

  /**
   * Get/alter the host of a Toolbox, i.e. is it in browser or in a separate
   * tab. See HostType for more details.
   */
  get hostType() {
    return this._host.type;
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
   * Get current zoom level of toolbox
   */
  get zoomValue() {
    return parseFloat(Services.prefs.getCharPref(ZOOM_PREF));
  },

  /**
   * Get the toolbox highlighter front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get highlighter() {
    if (this.highlighterUtils.isRemoteHighlightable) {
      return this._highlighter;
    } else {
      return null;
    }
  },

  /**
   * Get the toolbox's inspector front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get inspector() {
    return this._inspector;
  },

  /**
   * Get the toolbox's walker front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get walker() {
    return this._walker;
  },

  /**
   * Get the toolbox's node selection. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get selection() {
    return this._selection;
  },

  /**
   * Get the toggled state of the split console
   */
  get splitConsole() {
    return this._splitConsole;
  },

  /**
   * Open the toolbox
   */
  open: function() {
    let deferred = promise.defer();

    return this._host.create().then(iframe => {
      let deferred = promise.defer();

      let domReady = () => {
        this.isReady = true;

        let closeButton = this.doc.getElementById("toolbox-close");
        closeButton.addEventListener("command", this.destroy, true);

        this._buildDockButtons();
        this._buildOptions();
        this._buildTabs();
        this._buildButtons();
        this._addKeysToWindow();
        this._addToolSwitchingKeys();
        this._addZoomKeys();
        this._loadInitialZoom();

        // Load the toolbox-level actor fronts and utilities now
        this._target.makeRemote().then(() => {
          this._telemetry.toolOpened("toolbox");

          this.selectTool(this._defaultToolId).then(panel => {
            this.emit("ready");
            deferred.resolve();
          });
        });
      };

      iframe.setAttribute("src", this._URL);

      let domHelper = new DOMHelpers(iframe.contentWindow);
      domHelper.onceDOMReady(domReady);

      return deferred.promise;
    });
  },

  _buildOptions: function() {
    let key = this.doc.getElementById("toolbox-options-key");
    key.addEventListener("command", () => {
      this.selectTool("options");
    }, true);
  },

  _isResponsiveModeActive: function() {
    let responsiveModeActive = false;
    if (this.target.isLocalTab) {
      let tab = this.target.tab;
      let browserWindow = tab.ownerDocument.defaultView;
      let responsiveUIManager = browserWindow.ResponsiveUI.ResponsiveUIManager;
      responsiveModeActive = responsiveUIManager.isActiveForTab(tab);
    }
    return responsiveModeActive;
  },

  _splitConsoleOnKeypress: function(e) {
    let responsiveModeActive = this._isResponsiveModeActive();
    if (e.keyCode === e.DOM_VK_ESCAPE && !responsiveModeActive) {
      this.toggleSplitConsole();
    }
  },

  _addToolSwitchingKeys: function() {
    let nextKey = this.doc.getElementById("toolbox-next-tool-key");
    nextKey.addEventListener("command", this.selectNextTool.bind(this), true);
    let prevKey = this.doc.getElementById("toolbox-previous-tool-key");
    prevKey.addEventListener("command", this.selectPreviousTool.bind(this), true);

    // Split console uses keypress instead of command so the event can be
    // cancelled with stopPropagation on the keypress, and not preventDefault.
    this.doc.addEventListener("keypress", this._splitConsoleOnKeypress, false);
  },

  /**
   * Make sure that the console is showing up properly based on all the
   * possible conditions.
   *   1) If the console tab is selected, then regardless of split state
   *      it should take up the full height of the deck, and we should
   *      hide the deck and splitter.
   *   2) If the console tab is not selected and it is split, then we should
   *      show the splitter, deck, and console.
   *   3) If the console tab is not selected and it is *not* split,
   *      then we should hide the console and splitter, and show the deck
   *      at full height.
   */
  _refreshConsoleDisplay: function() {
    let deck = this.doc.getElementById("toolbox-deck");
    let webconsolePanel = this.doc.getElementById("toolbox-panel-webconsole");
    let splitter = this.doc.getElementById("toolbox-console-splitter");
    let openedConsolePanel = this.currentToolId === "webconsole";

    if (openedConsolePanel) {
      deck.setAttribute("collapsed", "true");
      splitter.setAttribute("hidden", "true");
      webconsolePanel.removeAttribute("collapsed");
    } else {
      deck.removeAttribute("collapsed");
      if (this._splitConsole) {
        webconsolePanel.removeAttribute("collapsed");
        splitter.removeAttribute("hidden");
      } else {
        webconsolePanel.setAttribute("collapsed", "true");
        splitter.setAttribute("hidden", "true");
      }
    }
  },

  /**
   * Wire up the listeners for the zoom keys.
   */
  _addZoomKeys: function() {
    let inKey = this.doc.getElementById("toolbox-zoom-in-key");
    inKey.addEventListener("command", this.zoomIn.bind(this), true);

    let inKey2 = this.doc.getElementById("toolbox-zoom-in-key2");
    inKey2.addEventListener("command", this.zoomIn.bind(this), true);

    let outKey = this.doc.getElementById("toolbox-zoom-out-key");
    outKey.addEventListener("command", this.zoomOut.bind(this), true);

    let resetKey = this.doc.getElementById("toolbox-zoom-reset-key");
    resetKey.addEventListener("command", this.zoomReset.bind(this), true);
  },

  /**
   * Set zoom on toolbox to whatever the last setting was.
   */
  _loadInitialZoom: function() {
    this.setZoom(this.zoomValue);
  },

  /**
   * Increase zoom level of toolbox window - make things bigger.
   */
  zoomIn: function() {
    this.setZoom(this.zoomValue + 0.1);
  },

  /**
   * Decrease zoom level of toolbox window - make things smaller.
   */
  zoomOut: function() {
    this.setZoom(this.zoomValue - 0.1);
  },

  /**
   * Reset zoom level of the toolbox window.
   */
  zoomReset: function() {
    this.setZoom(1);
  },

  /**
   * Set zoom level of the toolbox window.
   *
   * @param {number} zoomValue
   *        Zoom level e.g. 1.2
   */
  setZoom: function(zoomValue) {
    // cap zoom value
    zoomValue = Math.max(zoomValue, MIN_ZOOM);
    zoomValue = Math.min(zoomValue, MAX_ZOOM);

    let contViewer = this.frame.docShell.contentViewer;
    let docViewer = contViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);

    docViewer.fullZoom = zoomValue;

    Services.prefs.setCharPref(ZOOM_PREF, zoomValue);
  },

  /**
   * Adds the keys and commands to the Toolbox Window in window mode.
   */
  _addKeysToWindow: function() {
    if (this.hostType != Toolbox.HostType.WINDOW) {
      return;
    }

    let doc = this.doc.defaultView.parent.document;

    for (let [id, toolDefinition] of gDevTools.getToolDefinitionMap()) {
      // Prevent multiple entries for the same tool.
      if (!toolDefinition.key || doc.getElementById("key_" + id)) {
        continue;
      }

      let toolId = id;
      let key = doc.createElement("key");

      key.id = "key_" + toolId;

      if (toolDefinition.key.startsWith("VK_")) {
        key.setAttribute("keycode", toolDefinition.key);
      } else {
        key.setAttribute("key", toolDefinition.key);
      }

      key.setAttribute("modifiers", toolDefinition.modifiers);
      key.setAttribute("oncommand", "void(0);"); // needed. See bug 371900
      key.addEventListener("command", () => {
        this.selectTool(toolId).then(() => this.fireCustomKey(toolId));
      }, true);
      doc.getElementById("toolbox-keyset").appendChild(key);
    }

    // Add key for toggling the browser console from the detached window
    if (!doc.getElementById("key_browserconsole")) {
      let key = doc.createElement("key");
      key.id = "key_browserconsole";

      key.setAttribute("key", toolboxStrings("browserConsoleCmd.commandkey"));
      key.setAttribute("modifiers", "accel,shift");
      key.setAttribute("oncommand", "void(0)"); // needed. See bug 371900
      key.addEventListener("command", () => {
        HUDService.toggleBrowserConsole();
      }, true);
      doc.getElementById("toolbox-keyset").appendChild(key);
    }
  },

  /**
   * Handle any custom key events.  Returns true if there was a custom key binding run
   * @param {string} toolId
   *        Which tool to run the command on (skip if not current)
   */
  fireCustomKey: function(toolId) {
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolDefinition.onkey && this.currentToolId === toolId) {
      toolDefinition.onkey(this.getCurrentPanel());
    }
  },

  /**
   * Build the buttons for changing hosts. Called every time
   * the host changes.
   */
  _buildDockButtons: function() {
    let dockBox = this.doc.getElementById("toolbox-dock-buttons");

    while (dockBox.firstChild) {
      dockBox.removeChild(dockBox.firstChild);
    }

    if (!this._target.isLocalTab) {
      return;
    }

    let closeButton = this.doc.getElementById("toolbox-close");
    if (this.hostType == Toolbox.HostType.WINDOW) {
      closeButton.setAttribute("hidden", "true");
    } else {
      closeButton.removeAttribute("hidden");
    }

    let sideEnabled = Services.prefs.getBoolPref(this._prefs.SIDE_ENABLED);

    for (let type in Toolbox.HostType) {
      let position = Toolbox.HostType[type];
      if (position == this.hostType ||
          position == Toolbox.HostType.CUSTOM ||
          (!sideEnabled && position == Toolbox.HostType.SIDE)) {
        continue;
      }

      let button = this.doc.createElement("toolbarbutton");
      button.id = "toolbox-dock-" + position;
      button.className = "toolbox-dock-button";
      button.setAttribute("tooltiptext", toolboxStrings("toolboxDockButtons." +
                                                        position + ".tooltip"));
      button.addEventListener("command", () => {
        this.switchHost(position);
      });

      dockBox.appendChild(button);
    }
  },

  /**
   * Add tabs to the toolbox UI for registered tools
   */
  _buildTabs: function() {
    for (let definition of gDevTools.getToolDefinitionArray()) {
      this._buildTabForTool(definition);
    }
  },

  /**
   * Add buttons to the UI as specified in the devtools.toolbox.toolbarSpec pref
   */
  _buildButtons: function() {
    this._buildPickerButton();

    if (!this.target.isLocalTab) {
      return;
    }

    let spec = CommandUtils.getCommandbarSpec("devtools.toolbox.toolbarSpec");
    let env = CommandUtils.createEnvironment(this.target.tab.ownerDocument,
                                             this.target.window.document);
    let req = new Requisition(env);
    let buttons = CommandUtils.createButtons(spec, this._target, this.doc, req);
    let container = this.doc.getElementById("toolbox-buttons");
    buttons.forEach(container.appendChild.bind(container));
  },

  /**
   * Adding the element picker button is done here unlike the other buttons
   * since we want it to work for remote targets too
   */
  _buildPickerButton: function() {
    this._pickerButton = this.doc.createElement("toolbarbutton");
    this._pickerButton.id = "command-button-pick";
    this._pickerButton.className = "command-button";
    this._pickerButton.setAttribute("tooltiptext", toolboxStrings("pickButton.tooltip"));

    let container = this.doc.querySelector("#toolbox-buttons");
    container.appendChild(this._pickerButton);

    this._togglePicker = this.highlighterUtils.togglePicker.bind(this.highlighterUtils);
    this._pickerButton.addEventListener("command", this._togglePicker, false);
  },

  /**
   * Build a tab for one tool definition and add to the toolbox
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildTabForTool: function(toolDefinition) {
    if (!toolDefinition.isTargetSupported(this._target)) {
      return;
    }

    let tabs = this.doc.getElementById("toolbox-tabs");
    let deck = this.doc.getElementById("toolbox-deck");

    let id = toolDefinition.id;

    if (toolDefinition.ordinal == undefined || toolDefinition.ordinal < 0) {
      toolDefinition.ordinal = MAX_ORDINAL;
    }

    let radio = this.doc.createElement("radio");
    // The radio element is not being used in the conventional way, thus
    // the devtools-tab class replaces the radio XBL binding with its base
    // binding (the control-item binding).
    radio.className = "toolbox-tab devtools-tab";
    radio.id = "toolbox-tab-" + id;
    radio.setAttribute("toolid", id);
    radio.setAttribute("ordinal", toolDefinition.ordinal);
    radio.setAttribute("tooltiptext", toolDefinition.tooltip);

    radio.addEventListener("command", () => {
      this.selectTool(id);
    });

    // spacer lets us center the image and label, while allowing cropping
    let spacer = this.doc.createElement("spacer");
    spacer.setAttribute("flex", "1");
    radio.appendChild(spacer);

    if (toolDefinition.icon) {
      let image = this.doc.createElement("image");
      image.className = "default-icon";
      image.setAttribute("src",
                         toolDefinition.icon || toolDefinition.highlightedicon);
      radio.appendChild(image);
      // Adding the highlighted icon image
      image = this.doc.createElement("image");
      image.className = "highlighted-icon";
      image.setAttribute("src",
                         toolDefinition.highlightedicon || toolDefinition.icon);
      radio.appendChild(image);
    }

    if (toolDefinition.label) {
      let label = this.doc.createElement("label");
      label.setAttribute("value", toolDefinition.label)
      label.setAttribute("crop", "end");
      label.setAttribute("flex", "1");
      radio.appendChild(label);
      radio.setAttribute("flex", "1");
    }

    if (!toolDefinition.bgTheme) {
      toolDefinition.bgTheme = "theme-toolbar";
    }
    let vbox = this.doc.createElement("vbox");
    vbox.className = "toolbox-panel " + toolDefinition.bgTheme;

    // There is already a container for the webconsole frame.
    if (!this.doc.getElementById("toolbox-panel-" + id)) {
      vbox.id = "toolbox-panel-" + id;
    }

    // If there is no tab yet, or the ordinal to be added is the largest one.
    if (tabs.childNodes.length == 0 ||
        +tabs.lastChild.getAttribute("ordinal") <= toolDefinition.ordinal) {
      tabs.appendChild(radio);
      deck.appendChild(vbox);
    } else {
      // else, iterate over all the tabs to get the correct location.
      Array.some(tabs.childNodes, (node, i) => {
        if (+node.getAttribute("ordinal") > toolDefinition.ordinal) {
          tabs.insertBefore(radio, node);
          deck.insertBefore(vbox, deck.childNodes[i]);
          return true;
        }
        return false;
      });
    }

    this._addKeysToWindow();
  },

  /**
   * Ensure the tool with the given id is loaded.
   *
   * @param {string} id
   *        The id of the tool to load.
   */
  loadTool: function(id) {
    if (id === "inspector" && !this._inspector) {
      return this.initInspector().then(() => {
        return this.loadTool(id);
      });
    }

    let deferred = promise.defer();
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);

    if (iframe) {
      let panel = this._toolPanels.get(id);
      if (panel) {
        deferred.resolve(panel);
      } else {
        this.once(id + "-ready", panel => {
          deferred.resolve(panel);
        });
      }
      return deferred.promise;
    }

    let definition = gDevTools.getToolDefinition(id);
    if (!definition) {
      deferred.reject(new Error("no such tool id "+id));
      return deferred.promise;
    }

    iframe = this.doc.createElement("iframe");
    iframe.className = "toolbox-panel-iframe";
    iframe.id = "toolbox-panel-iframe-" + id;
    iframe.setAttribute("flex", 1);
    iframe.setAttribute("forceOwnRefreshDriver", "");
    iframe.tooltip = "aHTMLTooltip";
    iframe.style.visibility = "hidden";

    let vbox = this.doc.getElementById("toolbox-panel-" + id);
    vbox.appendChild(iframe);

    let onLoad = () => {
      // Prevent flicker while loading by waiting to make visible until now.
      iframe.style.visibility = "visible";

      let built = definition.build(iframe.contentWindow, this);
      promise.resolve(built).then((panel) => {
        this._toolPanels.set(id, panel);
        this.emit(id + "-ready", panel);
        gDevTools.emit(id + "-ready", this, panel);
        deferred.resolve(panel);
      });
    };

    iframe.setAttribute("src", definition.url);

    // Depending on the host, iframe.contentWindow is not always
    // defined at this moment. If it is not defined, we use an
    // event listener on the iframe DOM node. If it's defined,
    // we use the chromeEventHandler. We can't use a listener
    // on the DOM node every time because this won't work
    // if the (xul chrome) iframe is loaded in a content docshell.
    if (iframe.contentWindow) {
      let domHelper = new DOMHelpers(iframe.contentWindow);
      domHelper.onceDOMReady(onLoad);
    } else {
      let callback = () => {
        iframe.removeEventListener("DOMContentLoaded", callback);
        onLoad();
      }
      iframe.addEventListener("DOMContentLoaded", callback);
    }

    return deferred.promise;
  },

  /**
   * Switch to the tool with the given id
   *
   * @param {string} id
   *        The id of the tool to switch to
   */
  selectTool: function(id) {
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    if (selected) {
      selected.removeAttribute("selected");
    }

    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab.setAttribute("selected", "true");

    if (this.currentToolId == id) {
      // re-focus tool to get key events again
      this.focusTool(id);

      // Return the existing panel in order to have a consistent return value.
      return promise.resolve(this._toolPanels.get(id));
    }

    if (!this.isReady) {
      throw new Error("Can't select tool, wait for toolbox 'ready' event");
    }

    tab = this.doc.getElementById("toolbox-tab-" + id);

    if (tab) {
      if (this.currentToolId) {
        this._telemetry.toolClosed(this.currentToolId);
      }
      this._telemetry.toolOpened(id);
    } else {
      throw new Error("No tool found");
    }

    let tabstrip = this.doc.getElementById("toolbox-tabs");

    // select the right tab, making 0th index the default tab if right tab not
    // found
    let index = 0;
    let tabs = tabstrip.childNodes;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i] === tab) {
        index = i;
        break;
      }
    }
    tabstrip.selectedItem = tab;

    // and select the right iframe
    let deck = this.doc.getElementById("toolbox-deck");
    deck.selectedIndex = index;

    this.currentToolId = id;
    this._refreshConsoleDisplay();
    if (id != "options") {
      Services.prefs.setCharPref(this._prefs.LAST_TOOL, id);
    }

    return this.loadTool(id).then(panel => {
      // focus the tool's frame to start receiving key events
      this.focusTool(id);

      this.emit("select", id);
      this.emit(id + "-selected", panel);
      return panel;
    });
  },

  /**
   * Focus a tool's panel by id
   * @param  {string} id
   *         The id of tool to focus
   */
  focusTool: function(id) {
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);
    iframe.focus();
  },

  /**
   * Toggles the split state of the webconsole.  If the webconsole panel
   * is already selected, then this command is ignored.
   */
  toggleSplitConsole: function() {
    let openedConsolePanel = this.currentToolId === "webconsole";

    // Don't allow changes when console is open, since it could be confusing
    if (!openedConsolePanel) {
      this._splitConsole = !this._splitConsole;
      this._refreshConsoleDisplay();
      this.emit("split-console");

      if (this._splitConsole) {
        this.loadTool("webconsole").then(() => {
          this.focusTool("webconsole");
        });
      }
    }
  },

  /**
   * Loads the tool next to the currently selected tool.
   */
  selectNextTool: function() {
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    let next = selected.nextSibling || selected.parentNode.firstChild;
    let tool = next.getAttribute("toolid");
    return this.selectTool(tool);
  },

  /**
   * Loads the tool just left to the currently selected tool.
   */
  selectPreviousTool: function() {
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    let previous = selected.previousSibling || selected.parentNode.lastChild;
    let tool = previous.getAttribute("toolid");
    return this.selectTool(tool);
  },

  /**
   * Highlights the tool's tab if it is not the currently selected tool.
   *
   * @param {string} id
   *        The id of the tool to highlight
   */
  highlightTool: function(id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.setAttribute("highlighted", "true");
  },

  /**
   * De-highlights the tool's tab.
   *
   * @param {string} id
   *        The id of the tool to unhighlight
   */
  unhighlightTool: function(id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.removeAttribute("highlighted");
  },

  /**
   * Raise the toolbox host.
   */
  raise: function() {
    this._host.raise();
  },

  /**
   * Refresh the host's title.
   */
  _refreshHostTitle: function() {
    let toolName;
    let toolDef = gDevTools.getToolDefinition(this.currentToolId);
    if (toolDef) {
      toolName = toolDef.label;
    } else {
      // no tool is selected
      toolName = toolboxStrings("toolbox.defaultTitle");
    }
    let title = toolboxStrings("toolbox.titleTemplate",
                               toolName, this.target.url);
    this._host.setTitle(title);
  },

  /**
   * Create a host object based on the given host type.
   *
   * Warning: some hosts require that the toolbox target provides a reference to
   * the attached tab. Not all Targets have a tab property - make sure you correctly
   * mix and match hosts and targets.
   *
   * @param {string} hostType
   *        The host type of the new host object
   *
   * @return {Host} host
   *        The created host object
   */
  _createHost: function(hostType, options) {
    if (!Hosts[hostType]) {
      throw new Error("Unknown hostType: " + hostType);
    }

    // clean up the toolbox if its window is closed
    let newHost = new Hosts[hostType](this.target.tab, options);
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
  switchHost: function(hostType) {
    if (hostType == this._host.type || !this._target.isLocalTab) {
      return null;
    }

    let newHost = this._createHost(hostType);
    return newHost.create().then(iframe => {
      // change toolbox document's parent to the new host
      iframe.QueryInterface(Ci.nsIFrameLoaderOwner);
      iframe.swapFrameLoaders(this.frame);

      this._host.off("window-closed", this.destroy);
      this.destroyHost();

      this._host = newHost;

      if (this.hostType != Toolbox.HostType.CUSTOM) {
        Services.prefs.setCharPref(this._prefs.LAST_HOST, this._host.type);
      }

      this._buildDockButtons();
      this._addKeysToWindow();

      this.emit("host-changed");
    });
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} event
   *         Name of the event ("tool-registered")
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function(event, toolId) {
    let tool = gDevTools.getToolDefinition(toolId);
    this._buildTabForTool(tool);
  },

  /**
   * Handler for the tool-unregistered event.
   * @param  {string} event
   *         Name of the event ("tool-unregistered")
   * @param  {string|object} toolId
   *         Definition or id of the tool that was unregistered. Passing the
   *         tool id should be avoided as it is a temporary measure.
   */
  _toolUnregistered: function(event, toolId) {
    if (typeof toolId != "string") {
      toolId = toolId.id;
    }

    if (this._toolPanels.has(toolId)) {
      let instance = this._toolPanels.get(toolId);
      instance.destroy();
      this._toolPanels.delete(toolId);
    }

    let radio = this.doc.getElementById("toolbox-tab-" + toolId);
    let panel = this.doc.getElementById("toolbox-panel-" + toolId);

    if (radio) {
      if (this.currentToolId == toolId) {
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
      radio.parentNode.removeChild(radio);
    }

    if (panel) {
      panel.parentNode.removeChild(panel);
    }

    if (this.hostType == Toolbox.HostType.WINDOW) {
      let doc = this.doc.defaultView.parent.document;
      let key = doc.getElementById("key_" + toolId);
      if (key) {
        key.parentNode.removeChild(key);
      }
    }
  },

  /**
   * Initialize the inspector/walker/selection/highlighter fronts.
   * Returns a promise that resolves when the fronts are initialized
   */
  initInspector: function() {
    let deferred = promise.defer();

    if (!this._inspector) {
      this._inspector = InspectorFront(this._target.client, this._target.form);
      this._inspector.getWalker().then(walker => {
        this._walker = walker;
        this._selection = new Selection(this._walker);
        if (this.highlighterUtils.isRemoteHighlightable) {
          this._inspector.getHighlighter().then(highlighter => {
            this._highlighter = highlighter;
            deferred.resolve();
          });
        } else {
          deferred.resolve();
        }
      });
    } else {
      deferred.resolve();
    }

    return deferred.promise;
  },

  /**
   * Destroy the inspector/walker/selection fronts
   * Returns a promise that resolves when the fronts are destroyed
   */
  destroyInspector: function() {
    if (!this._inspector) {
      return promise.resolve();
    }

    let outstanding = () => {
      return Task.spawn(function*() {
        yield this.highlighterUtils.stopPicker();
        yield this._inspector.destroy();
        if (this._highlighter) {
          yield this._highlighter.destroy();
        }
        if (this._selection) {
          this._selection.destroy();
        }

        this._inspector = null;
        this._highlighter = null;
        this._selection = null;
        this._walker = null;
      }.bind(this));
    };

    // Releasing the walker (if it has been created)
    // This can fail, but in any case, we want to continue destroying the
    // inspector/highlighter/selection
    let walker = this._walker ? this._walker.release() : promise.resolve();
    return walker.then(outstanding, outstanding);
  },

  /**
   * Get the toolbox's notification box
   *
   * @return The notification box element.
   */
  getNotificationBox: function() {
    return this.doc.getElementById("toolbox-notificationbox");
  },

  /**
   * Destroy the current host, and remove event listeners from its frame.
   *
   * @return {promise} to be resolved when the host is destroyed.
   */
  destroyHost: function() {
    this.doc.removeEventListener("keypress",
      this._splitConsoleOnKeypress, false);
    return this._host.destroy();
  },

  /**
   * Remove all UI elements, detach from target and clear up
   */
  destroy: function() {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer;
    }

    this._target.off("navigate", this._refreshHostTitle);
    this.off("select", this._refreshHostTitle);
    this.off("host-changed", this._refreshHostTitle);

    gDevTools.off("tool-registered", this._toolRegistered);
    gDevTools.off("tool-unregistered", this._toolUnregistered);

    let outstanding = [];
    for (let [id, panel] of this._toolPanels) {
      try {
        outstanding.push(panel.destroy());
      } catch (e) {
        // We don't want to stop here if any panel fail to close.
        console.error("Panel " + id + ":", e);
      }
    }

    // Destroying the walker and inspector fronts
    outstanding.push(this.destroyInspector());
    // Removing buttons
    outstanding.push(() => {
      this._pickerButton.removeEventListener("command", this._togglePicker, false);
      this._pickerButton = null;
      let container = this.doc.getElementById("toolbox-buttons");
      while (container.firstChild) {
        container.removeChild(container.firstChild);
      }
    });
    // Remove the host UI
    outstanding.push(this.destroyHost());

    this._telemetry.destroy();

    return this._destroyer = promise.all(outstanding).then(() => {
      // Targets need to be notified that the toolbox is being torn down.
      // This is done after other destruction tasks since it may tear down
      // fronts and the debugger transport which earlier destroy methods may
      // require to complete.
      if (!this._target) {
        return null;
      }
      let target = this._target;
      this._target = null;
      target.off("close", this.destroy);
      return target.destroy();
    }).then(() => {
      this.emit("destroyed");
      // Free _host after the call to destroyed in order to let a chance
      // to destroyed listeners to still query toolbox attributes
      this._host = null;
      this._toolPanels.clear();
    }).then(null, console.error);
  }
};

/**
 * The ToolboxHighlighterUtils is what you should use for anything related to
 * node highlighting and picking.
 * It encapsulates the logic to connecting to the HighlighterActor.
 */
function ToolboxHighlighterUtils(toolbox) {
  this.toolbox = toolbox;
  this._onPickerNodeHovered = this._onPickerNodeHovered.bind(this);
  this._onPickerNodePicked = this._onPickerNodePicked.bind(this);
  this.stopPicker = this.stopPicker.bind(this);
}

ToolboxHighlighterUtils.prototype = {
  /**
   * Indicates whether the highlighter actor exists on the server.
   */
  get isRemoteHighlightable() {
    return this.toolbox._target.client.traits.highlightable;
  },

  /**
   * Start/stop the element picker on the debuggee target.
   */
  togglePicker: function() {
    if (this._isPicking) {
      return this.stopPicker();
    } else {
      return this.startPicker();
    }
  },

  _onPickerNodeHovered: function(res) {
    this.toolbox.emit("picker-node-hovered", res.node);
  },

  _onPickerNodePicked: function(res) {
    this.toolbox.selection.setNodeFront(res.node, "picker-node-picked");
    this.stopPicker();
  },

  /**
   * Start the element picker on the debuggee target.
   * This will request the inspector actor to start listening for mouse/touch
   * events on the target to highlight the hovered/picked element.
   * Depending on the server-side capabilities, this may fire events when nodes
   * are hovered.
   * @return A promise that resolves when the picker has started or immediately
   * if it is already started
   */
  startPicker: function() {
    if (this._isPicking) {
      return promise.resolve();
    }

    let deferred = promise.defer();

    let done = () => {
      this.toolbox.emit("picker-started");
      this.toolbox.on("select", this.stopPicker);
      deferred.resolve();
    };

    promise.all([
      this.toolbox.initInspector(),
      this.toolbox.selectTool("inspector")
    ]).then(() => {
      this._isPicking = true;
      this.toolbox._pickerButton.setAttribute("checked", "true");

      if (this.isRemoteHighlightable) {
        this.toolbox.highlighter.pick().then(done);

        this.toolbox.walker.on("picker-node-hovered", this._onPickerNodeHovered);
        this.toolbox.walker.on("picker-node-picked", this._onPickerNodePicked);
      } else {
        this.toolbox.walker.pick().then(node => {
          this.toolbox.selection.setNodeFront(node, "picker-node-picked");
          this.stopPicker();
        });
        done();
      }
    });

    return deferred.promise;
  },

  /**
   * Stop the element picker
   * @return A promise that resolves when the picker has stopped or immediately
   * if it is already stopped
   */
  stopPicker: function() {
    if (!this._isPicking) {
      return promise.resolve();
    }

    let deferred = promise.defer();

    let done = () => {
      this.toolbox.emit("picker-stopped");
      this.toolbox.off("select", this.stopPicker);
      deferred.resolve();
    };

    this.toolbox.initInspector().then(() => {
      this._isPicking = false;
      this.toolbox._pickerButton.removeAttribute("checked");
      if (this.isRemoteHighlightable) {
        this.toolbox.highlighter.cancelPick().then(done);
        this.toolbox.walker.off("picker-node-hovered", this._onPickerNodeHovered);
        this.toolbox.walker.off("picker-node-picked", this._onPickerNodePicked);
      } else {
        this.toolbox.walker.cancelPick().then(done);
      }
    });

    return deferred.promise;
  },

  /**
   * Show the box model highlighter on a node, given its NodeFront (this type
   * of front is normally returned by the WalkerActor).
   * @return a promise that resolves to the nodeFront when the node has been
   * highlit
   */
  highlightNodeFront: function(nodeFront, options={}) {
    let deferred = promise.defer();

    // If the remote highlighter exists on the target, use it
    if (this.isRemoteHighlightable) {
      this.toolbox.initInspector().then(() => {
        this.toolbox.highlighter.showBoxModel(nodeFront, options).then(() => {
          this.toolbox.emit("node-highlight", nodeFront);
          deferred.resolve(nodeFront);
        });
      });
    }
    // Else, revert to the "older" version of the highlighter in the walker
    // actor
    else {
      this.toolbox.walker.highlight(nodeFront).then(() => {
        this.toolbox.emit("node-highlight", nodeFront);
        deferred.resolve(nodeFront);
      });
    }

    return deferred.promise;
  },

  /**
   * This is a convenience method in case you don't have a nodeFront but a
   * valueGrip. This is often the case with VariablesView properties.
   * This method will simply translate the grip into a nodeFront and call
   * highlightNodeFront
   * @return a promise that resolves to the nodeFront when the node has been
   * highlit
   */
  highlightDomValueGrip: function(valueGrip, options={}) {
    return this._translateGripToNodeFront(valueGrip).then(nodeFront => {
      if (nodeFront) {
        return this.highlightNodeFront(nodeFront, options);
      } else {
        return promise.reject();
      }
    });
  },

  _translateGripToNodeFront: function(grip) {
    return this.toolbox.initInspector().then(() => {
      return this.toolbox.walker.getNodeActorFromObjectActor(grip.actor);
    });
  },

  /**
   * Hide the highlighter.
   * @return a promise that resolves when the highlighter is hidden
   */
  unhighlight: function() {
    if (this.isRemoteHighlightable) {
      // If the remote highlighter exists on the target, use it
      return this.toolbox.initInspector().then(() => {
        return this.toolbox.highlighter.hideBoxModel();
      });
    } else {
      // If not, no need to unhighlight as the older highlight method uses a
      // setTimeout to hide itself
      return promise.resolve();
    }
  }
};
