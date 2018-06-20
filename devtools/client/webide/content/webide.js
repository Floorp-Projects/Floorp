/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These files are loaded via webide.xul
/* import-globals-from project-panel.js */
/* import-globals-from runtime-panel.js */

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {gDevTools} = require("devtools/client/framework/devtools");
const {gDevToolsBrowser} = require("devtools/client/framework/devtools-browser");
const {Toolbox} = require("devtools/client/framework/toolbox");
const Services = require("Services");
const {AppProjects} = require("devtools/client/webide/modules/app-projects");
const {Connection} = require("devtools/shared/client/connection-manager");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const {GetAvailableAddons} = require("devtools/client/webide/modules/addons");
const {getJSON} = require("devtools/client/shared/getjson");
const Telemetry = require("devtools/client/shared/telemetry");
const {RuntimeScanners} = require("devtools/client/webide/modules/runtimes");
const {openContentLink} = require("devtools/client/shared/link");

const Strings =
  Services.strings.createBundle("chrome://devtools/locale/webide.properties");

const TELEMETRY_WEBIDE_IMPORT_PROJECT_COUNT = "DEVTOOLS_WEBIDE_IMPORT_PROJECT_COUNT";

const HELP_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Troubleshooting";

const MAX_ZOOM = 1.4;
const MIN_ZOOM = 0.6;

[["AppManager", AppManager],
 ["AppProjects", AppProjects],
 ["Connection", Connection]].forEach(([key, value]) => {
   Object.defineProperty(this, key, {
     value: value,
     enumerable: true,
     writable: false
   });
 });

// Download remote resources early
getJSON("devtools.webide.templatesURL");
getJSON("devtools.devices.url");

window.addEventListener("load", function() {
  UI.init();
}, {once: true});

window.addEventListener("unload", function() {
  UI.destroy();
}, {once: true});

var UI = {
  init: function() {
    this._telemetry = new Telemetry();
    this._telemetry.toolOpened("webide");

    AppManager.init();

    this.appManagerUpdate = this.appManagerUpdate.bind(this);
    AppManager.on("app-manager-update", this.appManagerUpdate);

    Cmds.showProjectPanel();
    Cmds.showRuntimePanel();

    this.updateCommands();

    this.onfocus = this.onfocus.bind(this);
    window.addEventListener("focus", this.onfocus, true);

    AppProjects.load().then(() => {
      this.autoSelectProject();
    }, e => {
      console.error(e);
      this.reportError("error_appProjectsLoadFailed");
    });

    // Auto install the ADB Addon Helper. Only once.
    // If the user decides to uninstall any of this addon, we won't install it again.
    const autoinstallADBHelper = Services.prefs.getBoolPref("devtools.webide.autoinstallADBHelper");
    if (autoinstallADBHelper) {
      const addons = GetAvailableAddons();
      addons.adb.install();
    }

    Services.prefs.setBoolPref("devtools.webide.autoinstallADBHelper", false);

    this.setupDeck();

    this.contentViewer = window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .contentViewer;
    this.contentViewer.fullZoom = Services.prefs.getCharPref("devtools.webide.zoom");

    gDevToolsBrowser.isWebIDEInitialized.resolve();
  },

  destroy: function() {
    window.removeEventListener("focus", this.onfocus, true);
    AppManager.off("app-manager-update", this.appManagerUpdate);
    AppManager.destroy();
    this.updateConnectionTelemetry();
    this._telemetry.toolClosed("webide");
  },

  onfocus: function() {
    // Because we can't track the activity in the folder project,
    // we need to validate the project regularly. Let's assume that
    // if a modification happened, it happened when the window was
    // not focused.
    if (AppManager.selectedProject &&
        AppManager.selectedProject.type != "mainProcess" &&
        AppManager.selectedProject.type != "runtimeApp" &&
        AppManager.selectedProject.type != "tab") {
      AppManager.validateAndUpdateProject(AppManager.selectedProject);
    }
  },

  appManagerUpdate: function(what, details) {
    // Got a message from app-manager.js
    // See AppManager.update() for descriptions of what these events mean.
    switch (what) {
      case "runtime-list":
        this.autoConnectRuntime();
        break;
      case "connection":
        this.updateRuntimeButton();
        this.updateCommands();
        this.updateConnectionTelemetry();
        break;
      case "project":
        this._updatePromise = (async function() {
          UI.updateTitle();
          await UI.destroyToolbox();
          UI.updateCommands();
          UI.openProject();
          await UI.autoStartProject();
          UI.autoOpenToolbox();
          UI.saveLastSelectedProject();
          UI.updateRemoveProjectButton();
        })();
        return;
      case "project-started":
        this.updateCommands();
        UI.autoOpenToolbox();
        break;
      case "project-stopped":
        UI.destroyToolbox();
        this.updateCommands();
        break;
      case "runtime-global-actors":
        // Check runtime version only on runtime-global-actors,
        // as we expect to use device actor
        this.checkRuntimeVersion();
        this.updateCommands();
        break;
      case "runtime-details":
        this.updateRuntimeButton();
        break;
      case "runtime":
        this.updateRuntimeButton();
        this.saveLastConnectedRuntime();
        break;
      case "project-validated":
        this.updateTitle();
        this.updateCommands();
        break;
      case "runtime-targets":
        this.autoSelectProject();
        break;
    }
    this._updatePromise = promise.resolve();
  },

  openInBrowser: function(url) {
    openContentLink(url);
  },

  updateTitle: function() {
    const project = AppManager.selectedProject;
    if (project) {
      window.document.title = Strings.formatStringFromName("title_app", [project.name], 1);
    } else {
      window.document.title = Strings.GetStringFromName("title_noApp");
    }
  },

  /** ******** BUSY UI **********/

  _busyTimeout: null,
  _busyOperationDescription: null,
  _busyPromise: null,

  busy: function() {
    const win = document.querySelector("window");
    win.classList.add("busy");
    win.classList.add("busy-undetermined");
    this.updateCommands();
    this.update("busy");
  },

  unbusy: function() {
    const win = document.querySelector("window");
    win.classList.remove("busy");
    win.classList.remove("busy-determined");
    win.classList.remove("busy-undetermined");
    this.updateCommands();
    this.update("unbusy");
    this._busyPromise = null;
  },

  setupBusyTimeout: function() {
    this.cancelBusyTimeout();
    this._busyTimeout = setTimeout(() => {
      this.unbusy();
      UI.reportError("error_operationTimeout", this._busyOperationDescription);
    }, Services.prefs.getIntPref("devtools.webide.busyTimeout"));
  },

  cancelBusyTimeout: function() {
    clearTimeout(this._busyTimeout);
  },

  busyWithProgressUntil: function(promise, operationDescription) {
    const busy = this.busyUntil(promise, operationDescription);
    const win = document.querySelector("window");
    const progress = document.querySelector("#action-busy-determined");
    progress.mode = "undetermined";
    win.classList.add("busy-determined");
    win.classList.remove("busy-undetermined");
    return busy;
  },

  busyUntil: function(promise, operationDescription) {
    // Freeze the UI until the promise is resolved. A timeout will unfreeze the
    // UI, just in case the promise never gets resolved.
    this._busyPromise = promise;
    this._busyOperationDescription = operationDescription;
    this.setupBusyTimeout();
    this.busy();
    promise.then(() => {
      this.cancelBusyTimeout();
      this.unbusy();
    }, (e) => {
      let message;
      if (e && e.error && e.message) {
        // Some errors come from fronts that are not based on protocol.js.
        // Errors are not translated to strings.
        message = operationDescription + " (" + e.error + "): " + e.message;
      } else {
        message = operationDescription + (e ? (": " + e) : "");
      }
      this.cancelBusyTimeout();
      const operationCanceled = e && e.canceled;
      if (!operationCanceled) {
        UI.reportError("error_operationFail", message);
        if (e) {
          console.error(e);
        }
      }
      this.unbusy();
    });
    return promise;
  },

  reportError: function(l10nProperty, ...l10nArgs) {
    let text;

    if (l10nArgs.length > 0) {
      text = Strings.formatStringFromName(l10nProperty, l10nArgs, l10nArgs.length);
    } else {
      text = Strings.GetStringFromName(l10nProperty);
    }

    console.error(text);

    const buttons = [{
      label: Strings.GetStringFromName("notification_showTroubleShooting_label"),
      accessKey: Strings.GetStringFromName("notification_showTroubleShooting_accesskey"),
      callback: function() {
        Cmds.showTroubleShooting();
      }
    }];

    const nbox = document.querySelector("#notificationbox");
    nbox.removeAllNotifications(true);
    nbox.appendNotification(text, "webide:errornotification", null,
                            nbox.PRIORITY_WARNING_LOW, buttons);
  },

  dismissErrorNotification: function() {
    const nbox = document.querySelector("#notificationbox");
    nbox.removeAllNotifications(true);
  },

  /** ******** COMMANDS **********/

  /**
   * This module emits various events when state changes occur.
   *
   * The events this module may emit include:
   *   busy:
   *     The window is currently busy and certain UI functions may be disabled.
   *   unbusy:
   *     The window is not busy and certain UI functions may be re-enabled.
   */
  update: function(what, details) {
    this.emit("webide-update", what, details);
  },

  updateCommands: function() {
    // Action commands
    const playCmd = document.querySelector("#cmd_play");
    const stopCmd = document.querySelector("#cmd_stop");
    const debugCmd = document.querySelector("#cmd_toggleToolbox");
    const playButton = document.querySelector("#action-button-play");
    const projectPanelCmd = document.querySelector("#cmd_showProjectPanel");

    if (document.querySelector("window").classList.contains("busy")) {
      playCmd.setAttribute("disabled", "true");
      stopCmd.setAttribute("disabled", "true");
      debugCmd.setAttribute("disabled", "true");
      projectPanelCmd.setAttribute("disabled", "true");
      return;
    }

    if (!AppManager.selectedProject || !AppManager.connected) {
      playCmd.setAttribute("disabled", "true");
      stopCmd.setAttribute("disabled", "true");
      debugCmd.setAttribute("disabled", "true");
    } else {
      const isProjectRunning = AppManager.isProjectRunning();
      if (isProjectRunning) {
        playButton.classList.add("reload");
        stopCmd.removeAttribute("disabled");
        debugCmd.removeAttribute("disabled");
      } else {
        playButton.classList.remove("reload");
        stopCmd.setAttribute("disabled", "true");
        debugCmd.setAttribute("disabled", "true");
      }

      // If connected and a project is selected
      if (AppManager.selectedProject.type == "runtimeApp") {
        playCmd.removeAttribute("disabled");
      } else if (AppManager.selectedProject.type == "tab") {
        playCmd.removeAttribute("disabled");
        stopCmd.setAttribute("disabled", "true");
      } else if (AppManager.selectedProject.type == "mainProcess") {
        playCmd.setAttribute("disabled", "true");
        stopCmd.setAttribute("disabled", "true");
      } else if (AppManager.selectedProject.errorsCount == 0 &&
            AppManager.runtimeCanHandleApps()) {
        playCmd.removeAttribute("disabled");
      } else {
        playCmd.setAttribute("disabled", "true");
      }
    }

    // Runtime commands
    const screenshotCmd = document.querySelector("#cmd_takeScreenshot");
    const detailsCmd = document.querySelector("#cmd_showRuntimeDetails");
    const disconnectCmd = document.querySelector("#cmd_disconnectRuntime");
    const devicePrefsCmd = document.querySelector("#cmd_showDevicePrefs");
    const settingsCmd = document.querySelector("#cmd_showSettings");

    if (AppManager.connected) {
      if (AppManager.deviceFront) {
        detailsCmd.removeAttribute("disabled");
        screenshotCmd.removeAttribute("disabled");
      }
      if (AppManager.preferenceFront) {
        devicePrefsCmd.removeAttribute("disabled");
      }
      disconnectCmd.removeAttribute("disabled");
    } else {
      detailsCmd.setAttribute("disabled", "true");
      screenshotCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      devicePrefsCmd.setAttribute("disabled", "true");
      settingsCmd.setAttribute("disabled", "true");
    }

    const runtimePanelButton = document.querySelector("#runtime-panel-button");

    if (AppManager.connected) {
      runtimePanelButton.setAttribute("active", "true");
      runtimePanelButton.removeAttribute("hidden");
    } else {
      runtimePanelButton.removeAttribute("active");
      runtimePanelButton.setAttribute("hidden", "true");
    }

    projectPanelCmd.removeAttribute("disabled");
  },

  updateRemoveProjectButton: function() {
    // Remove command
    const removeCmdNode = document.querySelector("#cmd_removeProject");
    if (AppManager.selectedProject) {
      removeCmdNode.removeAttribute("disabled");
    } else {
      removeCmdNode.setAttribute("disabled", "true");
    }
  },

  /** ******** RUNTIME **********/

  get lastConnectedRuntime() {
    return Services.prefs.getCharPref("devtools.webide.lastConnectedRuntime");
  },

  set lastConnectedRuntime(runtime) {
    Services.prefs.setCharPref("devtools.webide.lastConnectedRuntime", runtime);
  },

  autoConnectRuntime: function() {
    // Automatically reconnect to the previously selected runtime,
    // if available and has an ID and feature is enabled
    if (AppManager.selectedRuntime ||
        !Services.prefs.getBoolPref("devtools.webide.autoConnectRuntime") ||
        !this.lastConnectedRuntime) {
      return;
    }
    let [ , type, id] = this.lastConnectedRuntime.match(/^(\w+):(.+)$/);

    type = type.toLowerCase();

    // Local connection is mapped to AppManager.runtimeList.other array
    if (type == "local") {
      type = "other";
    }

    // We support most runtimes except simulator, that needs to be manually
    // launched
    if (type == "usb" || type == "wifi" || type == "other") {
      for (const runtime of AppManager.runtimeList[type]) {
        // Some runtimes do not expose an id and don't support autoconnect (like
        // remote connection)
        if (runtime.id == id) {
          // Only want one auto-connect attempt, so clear last runtime value
          this.lastConnectedRuntime = "";
          this.connectToRuntime(runtime);
        }
      }
    }
  },

  connectToRuntime: function(runtime) {
    const name = runtime.name;
    let promise = AppManager.connectToRuntime(runtime);
    promise.then(() => this.initConnectionTelemetry())
           .catch(() => {
             // Empty rejection handler to silence uncaught rejection warnings
             // |busyUntil| will listen for rejections.
             // Bug 1121100 may find a better way to silence these.
           });
    promise = this.busyUntil(promise, "Connecting to " + name);
    // Stop busy timeout for runtimes that take unknown or long amounts of time
    // to connect.
    if (runtime.prolongedConnection) {
      this.cancelBusyTimeout();
    }
    return promise;
  },

  updateRuntimeButton: function() {
    const labelNode = document.querySelector("#runtime-panel-button > .panel-button-label");
    if (!AppManager.selectedRuntime) {
      labelNode.setAttribute("value", Strings.GetStringFromName("runtimeButton_label"));
    } else {
      const name = AppManager.selectedRuntime.name;
      labelNode.setAttribute("value", name);
    }
  },

  saveLastConnectedRuntime: function() {
    if (AppManager.selectedRuntime &&
        AppManager.selectedRuntime.id !== undefined) {
      this.lastConnectedRuntime = AppManager.selectedRuntime.type + ":" +
                                  AppManager.selectedRuntime.id;
    } else {
      this.lastConnectedRuntime = "";
    }
  },

  /** ******** ACTIONS **********/

  _actionsToLog: new Set(),

  /**
   * For each new connection, track whether play and debug were ever used.  Only
   * one value is collected for each button, even if they are used multiple
   * times during a connection.
   */
  initConnectionTelemetry: function() {
    this._actionsToLog.add("play");
    this._actionsToLog.add("debug");
  },

  /**
   * Action occurred.  Log that it happened, and remove it from the loggable
   * set.
   */
  onAction: function(action) {
    if (!this._actionsToLog.has(action)) {
      return;
    }
    this.logActionState(action, true);
    this._actionsToLog.delete(action);
  },

  /**
   * Connection status changed or we are shutting down.  Record any loggable
   * actions as having not occurred.
   */
  updateConnectionTelemetry: function() {
    for (const action of this._actionsToLog.values()) {
      this.logActionState(action, false);
    }
    this._actionsToLog.clear();
  },

  logActionState: function(action, state) {
    const histogramId = "DEVTOOLS_WEBIDE_CONNECTION_" +
                      action.toUpperCase() + "_USED";
    this._telemetry.getHistogramById(histogramId).add(state);
  },

  /** ******** PROJECTS **********/

  openProject: function() {
    const project = AppManager.selectedProject;

    if (!project) {
      this.resetDeck();
      return;
    }

    this.selectDeckPanel("details");
  },

  async autoStartProject() {
    const project = AppManager.selectedProject;

    if (!project) {
      return;
    }
    if (!(project.type == "runtimeApp" ||
          project.type == "mainProcess" ||
          project.type == "tab")) {
      return; // For something that is not an editable app, we're done.
    }

    // Do not force opening apps that are already running, as they may have
    // some activity being opened and don't want to dismiss them.
    if (project.type == "runtimeApp" && !AppManager.isProjectRunning()) {
      await UI.busyUntil(AppManager.launchRuntimeApp(), "running app");
    }
  },

  async autoOpenToolbox() {
    const project = AppManager.selectedProject;

    if (!project) {
      return;
    }
    if (!(project.type == "runtimeApp" ||
          project.type == "mainProcess" ||
          project.type == "tab")) {
      return; // For something that is not an editable app, we're done.
    }

    await UI.createToolbox();
  },

  async importAndSelectApp(source) {
    const isPackaged = !!source.path;
    let project;
    try {
      project = await AppProjects[isPackaged ? "addPackaged" : "addHosted"](source);
    } catch (e) {
      if (e === "Already added") {
        // Select project that's already been added,
        // and allow it to be revalidated and selected
        project = AppProjects.get(isPackaged ? source.path : source);
      } else {
        throw e;
      }
    }

    // Select project
    AppManager.selectedProject = project;

    this._telemetry.getHistogramById(TELEMETRY_WEBIDE_IMPORT_PROJECT_COUNT).add(true);
  },

  // Remember the last selected project on the runtime
  saveLastSelectedProject: function() {
    const shouldRestore = Services.prefs.getBoolPref("devtools.webide.restoreLastProject");
    if (!shouldRestore) {
      return;
    }

    // Ignore unselection of project on runtime disconnection
    if (!AppManager.connected) {
      return;
    }

    let project = "", type = "";
    const selected = AppManager.selectedProject;
    if (selected) {
      if (selected.type == "runtimeApp") {
        type = "runtimeApp";
        project = selected.app.manifestURL;
      } else if (selected.type == "mainProcess") {
        type = "mainProcess";
      } else if (selected.type == "packaged" ||
                 selected.type == "hosted") {
        type = "local";
        project = selected.location;
      }
    }
    if (type) {
      Services.prefs.setCharPref("devtools.webide.lastSelectedProject",
                                 type + ":" + project);
    } else {
      Services.prefs.clearUserPref("devtools.webide.lastSelectedProject");
    }
  },

  autoSelectProject: function() {
    if (AppManager.selectedProject) {
      return;
    }
    const shouldRestore = Services.prefs.getBoolPref("devtools.webide.restoreLastProject");
    if (!shouldRestore) {
      return;
    }
    const pref = Services.prefs.getCharPref("devtools.webide.lastSelectedProject");
    if (!pref) {
      return;
    }
    const m = pref.match(/^(\w+):(.*)$/);
    if (!m) {
      return;
    }
    const [ , type, project] = m;

    if (type == "local") {
      const lastProject = AppProjects.get(project);
      if (lastProject) {
        AppManager.selectedProject = lastProject;
      }
    }

    // For other project types, we need to be connected to the runtime
    if (!AppManager.connected) {
      return;
    }

    if (type == "mainProcess" && AppManager.isMainProcessDebuggable()) {
      AppManager.selectedProject = {
        type: "mainProcess",
        name: Strings.GetStringFromName("mainProcess_label"),
        icon: AppManager.DEFAULT_PROJECT_ICON
      };
    } else if (type == "runtimeApp") {
      const app = AppManager.apps.get(project);
      if (app) {
        AppManager.selectedProject = {
          type: "runtimeApp",
          app: app.manifest,
          icon: app.iconURL,
          name: app.manifest.name
        };
      }
    }
  },

  /** ******** DECK **********/

  setupDeck: function() {
    const iframes = document.querySelectorAll("#deck > iframe");
    for (const iframe of iframes) {
      iframe.tooltip = "aHTMLTooltip";
    }
  },

  resetFocus: function() {
    document.commandDispatcher.focusedElement = document.documentElement;
  },

  selectDeckPanel: function(id) {
    const deck = document.querySelector("#deck");
    if (deck.selectedPanel && deck.selectedPanel.id === "deck-panel-" + id) {
      // This panel is already displayed.
      return;
    }
    this.resetFocus();
    const panel = deck.querySelector("#deck-panel-" + id);
    const lazysrc = panel.getAttribute("lazysrc");
    if (lazysrc) {
      panel.removeAttribute("lazysrc");
      panel.setAttribute("src", lazysrc);
    }
    deck.selectedPanel = panel;
  },

  resetDeck: function() {
    this.resetFocus();
    const deck = document.querySelector("#deck");
    deck.selectedPanel = null;
  },

  async checkRuntimeVersion() {
    if (AppManager.connected) {
      const { client } = AppManager.connection;
      const report = await client.checkRuntimeVersion(AppManager.listTabsForm);
      if (report.incompatible == "too-recent") {
        this.reportError("error_runtimeVersionTooRecent", report.runtimeID,
          report.localID);
      }
      if (report.incompatible == "too-old") {
        this.reportError("error_runtimeVersionTooOld", report.runtimeVersion,
          report.minVersion);
      }
    }
  },

  /** ******** TOOLBOX **********/

  /**
   * There are many ways to close a toolbox:
   *   * Close button inside the toolbox
   *   * Toggle toolbox wrench in WebIDE
   *   * Disconnect the current runtime gracefully
   *   * Yank cord out of device
   *   * Close or crash the app/tab
   * We can't know for sure which one was used here, so reset the
   * |toolboxPromise| since someone must be destroying it to reach here,
   * and call our own close method.
   */
  _onToolboxClosed: function(promise, iframe) {
    // Only save toolbox size, disable wrench button, workaround focus issue...
    // if we are closing the last toolbox:
    //  - toolboxPromise is nullified by destroyToolbox and is still null here
    //    if no other toolbox has been opened in between,
    //  - having two distinct promise means we are receiving closed event
    //    for a previous, non-current, toolbox.
    if (!this.toolboxPromise || this.toolboxPromise === promise) {
      this.toolboxPromise = null;
      this.resetFocus();
      Services.prefs.setIntPref("devtools.toolbox.footer.height", iframe.height);

      const splitter = document.querySelector(".devtools-horizontal-splitter");
      splitter.setAttribute("hidden", "true");
      document.querySelector("#action-button-debug").removeAttribute("active");
    }
    // We have to destroy the iframe, otherwise, the keybindings of webide don't work
    // properly anymore.
    iframe.remove();
  },

  destroyToolbox: function() {
    // Only have a live toolbox if |this.toolboxPromise| exists
    if (this.toolboxPromise) {
      const toolboxPromise = this.toolboxPromise;
      this.toolboxPromise = null;
      return toolboxPromise.then(toolbox => toolbox.destroy());
    }
    return promise.resolve();
  },

  createToolbox: function() {
    // If |this.toolboxPromise| exists, there is already a live toolbox
    if (this.toolboxPromise) {
      return this.toolboxPromise;
    }

    const iframe = document.createElement("iframe");
    iframe.id = "toolbox";

    // Compute a uid on the iframe in order to identify toolbox iframe
    // when receiving toolbox-close event
    iframe.uid = new Date().getTime();

    const height = Services.prefs.getIntPref("devtools.toolbox.footer.height");
    iframe.height = height;

    const promise = this.toolboxPromise = AppManager.getTarget().then(target => {
      return this._showToolbox(target, iframe);
    }).then(toolbox => {
      // Destroy the toolbox on WebIDE side before
      // toolbox.destroy's promise resolves.
      toolbox.once("destroyed", this._onToolboxClosed.bind(this, promise, iframe));
      return toolbox;
    }, console.error);

    return this.busyUntil(this.toolboxPromise, "opening toolbox");
  },

  _showToolbox: function(target, iframe) {
    const splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.removeAttribute("hidden");

    document.querySelector("notificationbox").insertBefore(iframe, splitter.nextSibling);
    const host = Toolbox.HostType.CUSTOM;
    const options = { customIframe: iframe, zoom: false, uid: iframe.uid };

    document.querySelector("#action-button-debug").setAttribute("active", "true");

    return gDevTools.showToolbox(target, null, host, options);
  },
};

EventEmitter.decorate(UI);

var Cmds = {
  quit: function() {
    window.close();
  },

  showProjectPanel: function() {
    ProjectPanel.toggleSidebar();
    return promise.resolve();
  },

  showRuntimePanel: function() {
    RuntimeScanners.scan();
    RuntimePanel.toggleSidebar();
  },

  disconnectRuntime: function() {
    const disconnecting = (async function() {
      await UI.destroyToolbox();
      await AppManager.disconnectRuntime();
    })();
    return UI.busyUntil(disconnecting, "disconnecting from runtime");
  },

  takeScreenshot: function() {
    const url = AppManager.deviceFront.screenshotToDataURL();
    return UI.busyUntil(url.then(longstr => {
      return longstr.string().then(dataURL => {
        longstr.release().catch(console.error);
        UI.openInBrowser(dataURL);
      });
    }), "taking screenshot");
  },

  showRuntimeDetails: function() {
    UI.selectDeckPanel("runtimedetails");
  },

  showDevicePrefs: function() {
    UI.selectDeckPanel("devicepreferences");
  },

  async play() {
    let busy;
    switch (AppManager.selectedProject.type) {
      case "packaged":
        busy = UI.busyWithProgressUntil(AppManager.installAndRunProject(),
                                        "installing and running app");
        break;
      case "hosted":
        busy = UI.busyUntil(AppManager.installAndRunProject(),
                            "installing and running app");
        break;
      case "runtimeApp":
        busy = UI.busyUntil(AppManager.launchOrReloadRuntimeApp(), "launching / reloading app");
        break;
      case "tab":
        busy = UI.busyUntil(AppManager.reloadTab(), "reloading tab");
        break;
    }
    if (!busy) {
      return promise.reject();
    }
    UI.onAction("play");
    return busy;
  },

  stop: function() {
    return UI.busyUntil(AppManager.stopRunningApp(), "stopping app");
  },

  toggleToolbox: function() {
    UI.onAction("debug");
    if (UI.toolboxPromise) {
      UI.destroyToolbox();
      return promise.resolve();
    }
    return UI.createToolbox();
  },

  removeProject: function() {
    AppManager.removeSelectedProject();
  },

  showTroubleShooting: function() {
    UI.openInBrowser(HELP_URL);
  },

  showAddons: function() {
    UI.selectDeckPanel("addons");
  },

  showPrefs: function() {
    UI.selectDeckPanel("prefs");
  },

  zoomIn: function() {
    if (UI.contentViewer.fullZoom < MAX_ZOOM) {
      UI.contentViewer.fullZoom += 0.1;
      Services.prefs.setCharPref("devtools.webide.zoom", UI.contentViewer.fullZoom);
    }
  },

  zoomOut: function() {
    if (UI.contentViewer.fullZoom > MIN_ZOOM) {
      UI.contentViewer.fullZoom -= 0.1;
      Services.prefs.setCharPref("devtools.webide.zoom", UI.contentViewer.fullZoom);
    }
  },

  resetZoom: function() {
    UI.contentViewer.fullZoom = 1;
    Services.prefs.setCharPref("devtools.webide.zoom", 1);
  }
};
