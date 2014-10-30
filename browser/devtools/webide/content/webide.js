/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {AppProjects} = require("devtools/app-manager/app-projects");
const {Connection} = require("devtools/client/connection-manager");
const {AppManager} = require("devtools/webide/app-manager");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const ProjectEditor = require("projecteditor/projecteditor");
const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {GetAvailableAddons} = require("devtools/webide/addons");
const {GetTemplatesJSON, GetAddonsJSON} = require("devtools/webide/remote-resources");
const utils = require("devtools/webide/utils");
const Telemetry = require("devtools/shared/telemetry");
const {RuntimeScanners, WiFiScanner} = require("devtools/webide/runtimes");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

const HTML = "http://www.w3.org/1999/xhtml";
const HELP_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Troubleshooting";

// download template index early
GetTemplatesJSON(true);

// See bug 989619
console.log = console.log.bind(console);
console.warn = console.warn.bind(console);
console.error = console.error.bind(console);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  UI.init();
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  UI.uninit();
});

let UI = {
  init: function() {
    this._telemetry = new Telemetry();
    this._telemetry.toolOpened("webide");

    AppManager.init();

    this.onMessage = this.onMessage.bind(this);
    window.addEventListener("message", this.onMessage);

    this.appManagerUpdate = this.appManagerUpdate.bind(this);
    AppManager.on("app-manager-update", this.appManagerUpdate);

    this.updateCommands();
    this.updateRuntimeList();

    this.onfocus = this.onfocus.bind(this);
    window.addEventListener("focus", this.onfocus, true);

    AppProjects.load().then(() => {
      this.autoSelectProject();
    });

    // Auto install the ADB Addon Helper and Tools Adapters. Only once.
    // If the user decides to uninstall any of this addon, we won't install it again.
    let autoinstallADBHelper = Services.prefs.getBoolPref("devtools.webide.autoinstallADBHelper");
    let autoinstallFxdtAdapters = Services.prefs.getBoolPref("devtools.webide.autoinstallFxdtAdapters");
    if (autoinstallADBHelper) {
      GetAvailableAddons().then(addons => {
        addons.adb.install();
      }, console.error);
    }
    if (autoinstallFxdtAdapters) {
      GetAvailableAddons().then(addons => {
        addons.adapters.install();
      }, console.error);
    }
    Services.prefs.setBoolPref("devtools.webide.autoinstallADBHelper", false);
    Services.prefs.setBoolPref("devtools.webide.autoinstallFxdtAdapters", false);

    this.lastConnectedRuntime = Services.prefs.getCharPref("devtools.webide.lastConnectedRuntime");

    if (Services.prefs.getBoolPref("devtools.webide.widget.autoinstall") &&
        !Services.prefs.getBoolPref("devtools.webide.widget.enabled")) {
      Services.prefs.setBoolPref("devtools.webide.widget.enabled", true);
      gDevToolsBrowser.moveWebIDEWidgetInNavbar();
    }

    this.setupDeck();
  },

  uninit: function() {
    window.removeEventListener("focus", this.onfocus, true);
    AppManager.off("app-manager-update", this.appManagerUpdate);
    AppManager.uninit();
    window.removeEventListener("message", this.onMessage);
    this.updateConnectionTelemetry();
    this._telemetry.toolClosed("webide");
  },

  canWindowClose: function() {
    if (this.projecteditor) {
      return this.projecteditor.confirmUnsaved();
    }
    return true;
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
      AppManager.validateProject(AppManager.selectedProject);
    }
  },

  appManagerUpdate: function(event, what, details) {
    // Got a message from app-manager.js
    switch (what) {
      case "runtimelist":
        this.updateRuntimeList();
        this.autoConnectRuntime();
        break;
      case "connection":
        this.updateRuntimeButton();
        this.updateCommands();
        this.updateConnectionTelemetry();
        break;
      case "project":
        this._updatePromise = Task.spawn(function() {
          UI.updateTitle();
          yield UI.destroyToolbox();
          UI.updateCommands();
          UI.updateProjectButton();
          UI.openProject();
          UI.autoStartProject();
          UI.saveLastSelectedProject();
        });
        return;
      case "project-is-not-running":
      case "project-is-running":
      case "list-tabs-response":
        this.updateCommands();
        break;
      case "runtime-details":
        this.updateRuntimeButton();
        break;
      case "runtime-changed":
        this.updateRuntimeButton();
        this.saveLastConnectedRuntime();
        break;
      case "project-validated":
        this.updateTitle();
        this.updateCommands();
        this.updateProjectButton();
        this.updateProjectEditorHeader();
        break;
      case "install-progress":
        this.updateProgress(Math.round(100 * details.bytesSent / details.totalBytes));
        break;
      case "runtime-apps-found":
        this.autoSelectProject();
        break;
    };
    this._updatePromise = promise.resolve();
  },

  openInBrowser: function(url) {
    // Open a URL in a Firefox window
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWin) {
      let gBrowser = browserWin.gBrowser;
      gBrowser.selectedTab = gBrowser.addTab(url);
      browserWin.focus();
    } else {
      window.open(url);
    }
  },

  updateTitle: function() {
    let project = AppManager.selectedProject;
    if (project) {
      window.document.title = Strings.formatStringFromName("title_app", [project.name], 1);
    } else {
      window.document.title = Strings.GetStringFromName("title_noApp");
    }
  },

  hidePanels: function() {
    let panels = document.querySelectorAll("panel");
    for (let p of panels) {
      p.hidePopup();
    }
  },

  /********** BUSY UI **********/

  _busyTimeout: null,
  _busyOperationDescription: null,
  _busyPromise: null,

  updateProgress: function(percent) {
    let progress = document.querySelector("#action-busy-determined");
    progress.mode = "determined";
    progress.value = percent;
    this.setupBusyTimeout();
  },

  busy: function() {
    this.hidePanels();
    let win = document.querySelector("window");
    win.classList.add("busy")
    win.classList.add("busy-undetermined");
    this.updateCommands();
  },

  unbusy: function() {
    let win = document.querySelector("window");
    win.classList.remove("busy")
    win.classList.remove("busy-determined");
    win.classList.remove("busy-undetermined");
    this.updateCommands();
    this._busyPromise = null;
  },

  setupBusyTimeout: function() {
    this.cancelBusyTimeout();
    this._busyTimeout = setTimeout(() => {
      this.unbusy();
      UI.reportError("error_operationTimeout", this._busyOperationDescription);
    }, 6000);
  },

  cancelBusyTimeout: function() {
    clearTimeout(this._busyTimeout);
  },

  busyWithProgressUntil: function(promise, operationDescription) {
    let busy = this.busyUntil(promise, operationDescription);
    let win = document.querySelector("window");
    let progress = document.querySelector("#action-busy-determined");
    progress.mode = "undetermined";
    win.classList.add("busy-determined");
    win.classList.remove("busy-undetermined");
    return busy;
  },

  busyUntil: function(promise, operationDescription) {
    // Freeze the UI until the promise is resolved. A 30s timeout
    // will unfreeze the UI, just in case the promise never gets
    // resolved.
    this._busyPromise = promise;
    this._busyOperationDescription = operationDescription;
    this.setupBusyTimeout();
    this.busy();
    promise.then(() => {
      this.cancelBusyTimeout();
      this.unbusy();
    }, (e) => {
      let message = operationDescription + (e ? (": " + e) : "");
      this.cancelBusyTimeout();
      let operationCanceled = e && e.canceled;
      if (!operationCanceled) {
        UI.reportError("error_operationFail", message);
        console.error(e);
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

    let buttons = [{
      label: Strings.GetStringFromName("notification_showTroubleShooting_label"),
      accessKey: Strings.GetStringFromName("notification_showTroubleShooting_accesskey"),
      callback: function () {
        Cmds.showTroubleShooting();
      }
    }];

    let nbox = document.querySelector("#notificationbox");
    nbox.removeAllNotifications(true);
    nbox.appendNotification(text, "webide:errornotification", null,
                            nbox.PRIORITY_WARNING_LOW, buttons);
  },

  dismissErrorNotification: function() {
    let nbox = document.querySelector("#notificationbox");
    nbox.removeAllNotifications(true);
  },

  /********** RUNTIME **********/

  updateRuntimeList: function() {
    let wifiHeaderNode = document.querySelector("#runtime-header-wifi");
    if (WiFiScanner.allowed) {
      wifiHeaderNode.removeAttribute("hidden");
    } else {
      wifiHeaderNode.setAttribute("hidden", "true");
    }

    let usbListNode = document.querySelector("#runtime-panel-usb");
    let wifiListNode = document.querySelector("#runtime-panel-wifi");
    let simulatorListNode = document.querySelector("#runtime-panel-simulator");
    let otherListNode = document.querySelector("#runtime-panel-other");

    let noHelperNode = document.querySelector("#runtime-panel-noadbhelper");
    let noUSBNode = document.querySelector("#runtime-panel-nousbdevice");

    if (Devices.helperAddonInstalled) {
      noHelperNode.setAttribute("hidden", "true");
    } else {
      noHelperNode.removeAttribute("hidden");
    }

    let runtimeList = AppManager.runtimeList;

    if (runtimeList.usb.length === 0 && Devices.helperAddonInstalled) {
      noUSBNode.removeAttribute("hidden");
    } else {
      noUSBNode.setAttribute("hidden", "true");
    }

    for (let [type, parent] of [
      ["usb", usbListNode],
      ["wifi", wifiListNode],
      ["simulator", simulatorListNode],
      ["other", otherListNode],
    ]) {
      while (parent.hasChildNodes()) {
        parent.firstChild.remove();
      }
      for (let runtime of runtimeList[type]) {
        let panelItemNode = document.createElement("toolbarbutton");
        panelItemNode.className = "panel-item runtime-panel-item-" + type;
        panelItemNode.setAttribute("label", runtime.name);
        parent.appendChild(panelItemNode);
        let r = runtime;
        panelItemNode.addEventListener("click", () => {
          this.hidePanels();
          this.dismissErrorNotification();
          this.connectToRuntime(r);
        }, true);
      }
    }
  },

  autoConnectRuntime: function () {
    // Automatically reconnect to the previously selected runtime,
    // if available and has an ID
    if (AppManager.selectedRuntime || !this.lastConnectedRuntime) {
      return;
    }
    let [_, type, id] = this.lastConnectedRuntime.match(/^(\w+):(.+)$/);

    type = type.toLowerCase();

    // Local connection is mapped to AppManager.runtimeList.other array
    if (type == "local") {
      type = "other";
    }

    // We support most runtimes except simulator, that needs to be manually
    // launched
    if (type == "usb" || type == "wifi" || type == "other") {
      for (let runtime of AppManager.runtimeList[type]) {
        // Some runtimes do not expose an id and don't support autoconnect (like
        // remote connection)
        if (runtime.id == id) {
          this.connectToRuntime(runtime);
        }
      }
    }
  },

  connectToRuntime: function(runtime) {
    let name = runtime.name;
    let promise = AppManager.connectToRuntime(runtime);
    promise.then(() => this.initConnectionTelemetry());
    return this.busyUntil(promise, "connecting to runtime " + name);
  },

  updateRuntimeButton: function() {
    let labelNode = document.querySelector("#runtime-panel-button > .panel-button-label");
    if (!AppManager.selectedRuntime) {
      labelNode.setAttribute("value", Strings.GetStringFromName("runtimeButton_label"));
    } else {
      let name = AppManager.selectedRuntime.name;
      labelNode.setAttribute("value", name);
    }
  },

  saveLastConnectedRuntime: function () {
    if (AppManager.selectedRuntime &&
        AppManager.selectedRuntime.id !== undefined) {
      this.lastConnectedRuntime = AppManager.selectedRuntime.type + ":" +
                                  AppManager.selectedRuntime.id;
    } else {
      this.lastConnectedRuntime = "";
    }
    Services.prefs.setCharPref("devtools.webide.lastConnectedRuntime",
                               this.lastConnectedRuntime);
  },

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
    for (let action of this._actionsToLog.values()) {
      this.logActionState(action, false);
    }
    this._actionsToLog.clear();
  },

  logActionState: function(action, state) {
    let histogramId = "DEVTOOLS_WEBIDE_CONNECTION_" +
                      action.toUpperCase() + "_USED";
    this._telemetry.log(histogramId, state);
  },

  /********** PROJECTS **********/

  // Panel & button

  updateProjectButton: function() {
    let buttonNode = document.querySelector("#project-panel-button");
    let labelNode = buttonNode.querySelector(".panel-button-label");
    let imageNode = buttonNode.querySelector(".panel-button-image");

    let project = AppManager.selectedProject;

    if (!project) {
      buttonNode.classList.add("no-project");
      labelNode.setAttribute("value", Strings.GetStringFromName("projectButton_label"));
      imageNode.removeAttribute("src");
    } else {
      buttonNode.classList.remove("no-project");
      labelNode.setAttribute("value", project.name);
      imageNode.setAttribute("src", project.icon);
    }
  },

  // ProjectEditor & details screen

  destroyProjectEditor: function() {
    if (this.projecteditor) {
      this.projecteditor.destroy();
      this.projecteditor = null;
    }
  },

  updateProjectEditorMenusVisibility: function() {
    if (this.projecteditor) {
      let panel = document.querySelector("#deck").selectedPanel;
      if (panel && panel.id == "deck-panel-projecteditor") {
        this.projecteditor.menuEnabled = true;
      } else {
        this.projecteditor.menuEnabled = false;
      }
    }
  },

  getProjectEditor: function() {
    if (this.projecteditor) {
      return this.projecteditor.loaded;
    }

    let projecteditorIframe = document.querySelector("#deck-panel-projecteditor");
    this.projecteditor = ProjectEditor.ProjectEditor(projecteditorIframe, {
      menubar: document.querySelector("#main-menubar"),
      menuindex: 1
    });
    this.projecteditor.on("onEditorSave", (editor, resource) => {
      AppManager.validateProject(AppManager.selectedProject);
    });
    return this.projecteditor.loaded;
  },

  updateProjectEditorHeader: function() {
    let project = AppManager.selectedProject;
    if (!project || !this.projecteditor) {
      return;
    }
    let status = project.validationStatus || "unknown";
    if (status == "error warning") {
      status = "error";
    }
    this.getProjectEditor().then((projecteditor) => {
      projecteditor.setProjectToAppPath(project.location, {
        name: project.name,
        iconUrl: project.icon,
        projectOverviewURL: "chrome://webide/content/details.xhtml",
        validationStatus: status
      }).then(null, console.error);
    }, console.error);
  },

  isProjectEditorEnabled: function() {
    return Services.prefs.getBoolPref("devtools.webide.showProjectEditor");
  },

  openProject: function() {
    let project = AppManager.selectedProject;

    // Nothing to show

    if (!project) {
      this.resetDeck();
      return;
    }

    // Make sure the directory exist before we show Project Editor

    let forceDetailsOnly = false;
    if (project.type == "packaged") {
      forceDetailsOnly = !utils.doesFileExist(project.location);
    }

    // Show only the details screen

    if (project.type != "packaged" ||
        !this.isProjectEditorEnabled() ||
        forceDetailsOnly) {
      this.selectDeckPanel("details");
      return;
    }

    // Show ProjectEditor

    this.selectDeckPanel("projecteditor");

    this.getProjectEditor().then(() => {
      this.updateProjectEditorHeader();
    }, console.error);
  },

  autoStartProject: function() {
    let project = AppManager.selectedProject;

    if (!project) {
      return;
    }
    if (!(project.type == "runtimeApp" ||
          project.type == "mainProcess" ||
          project.type == "tab")) {
      return; // For something that is not an editable app, we're done.
    }

    Task.spawn(function() {
      if (project.type == "runtimeApp") {
        yield UI.busyUntil(AppManager.launchRuntimeApp(), "running app");
      }
      yield UI.createToolbox();
    });
  },

  importAndSelectApp: Task.async(function* (source) {
    let isPackaged = !!source.path;
    let project;
    try {
      project = yield AppProjects[isPackaged ? "addPackaged" : "addHosted"](source);
    } catch (e) {
      if (e === "Already added") {
        // Select project that's already been added,
        // and allow it to be revalidated and selected
        project = AppProjects.get(isPackaged ? source.path : source);
      } else {
        throw e;
      }
    }

    // Validate project
    yield AppManager.validateProject(project);

    // Select project
    AppManager.selectedProject = project;
  }),

  // Remember the last selected project on the runtime
  saveLastSelectedProject: function() {
    let shouldRestore = Services.prefs.getBoolPref("devtools.webide.restoreLastProject");
    if (!shouldRestore) {
      return;
    }

    // Ignore unselection of project on runtime disconnection
    if (AppManager.connection.status != Connection.Status.CONNECTED) {
      return;
    }

    let project = "", type = "";
    let selected = AppManager.selectedProject;
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
    let shouldRestore = Services.prefs.getBoolPref("devtools.webide.restoreLastProject");
    if (!shouldRestore) {
      return;
    }
    let pref = Services.prefs.getCharPref("devtools.webide.lastSelectedProject");
    if (!pref) {
      return;
    }
    let m = pref.match(/^(\w+):(.*)$/);
    if (!m) {
      return;
    }
    let [_, type, project] = m;

    if (type == "local") {
      let lastProject = AppProjects.get(project);
      if (lastProject) {
        AppManager.selectedProject = lastProject;
      }
    }

    // For other project types, we need to be connected to the runtime
    if (AppManager.connection.status != Connection.Status.CONNECTED) {
      return;
    }

    if (type == "mainProcess" && AppManager.isMainProcessDebuggable()) {
      AppManager.selectedProject = {
        type: "mainProcess",
        name: Strings.GetStringFromName("mainProcess_label"),
        icon: AppManager.DEFAULT_PROJECT_ICON
      }
    } else if (type == "runtimeApp") {
      let app = AppManager.apps.get(project);
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

  /********** DECK **********/

  setupDeck: function() {
    let iframes = document.querySelectorAll("#deck > iframe");
    for (let iframe of iframes) {
      iframe.tooltip = "aHTMLTooltip";
    }
  },

  resetFocus: function() {
    document.commandDispatcher.focusedElement = document.documentElement;
  },

  selectDeckPanel: function(id) {
    this.hidePanels();
    this.resetFocus();
    let deck = document.querySelector("#deck");
    let panel = deck.querySelector("#deck-panel-" + id);
    let lazysrc = panel.getAttribute("lazysrc");
    if (lazysrc) {
      panel.removeAttribute("lazysrc");
      panel.setAttribute("src", lazysrc);
    }
    deck.selectedPanel = panel;
    this.updateProjectEditorMenusVisibility();
  },

  resetDeck: function() {
    this.resetFocus();
    let deck = document.querySelector("#deck");
    deck.selectedPanel = null;
    this.updateProjectEditorMenusVisibility();
  },

  /********** COMMANDS **********/

  updateCommands: function() {

    if (document.querySelector("window").classList.contains("busy")) {
      document.querySelector("#cmd_newApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_importPackagedApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_importHostedApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_showProjectPanel").setAttribute("disabled", "true");
      document.querySelector("#cmd_showRuntimePanel").setAttribute("disabled", "true");
      document.querySelector("#cmd_removeProject").setAttribute("disabled", "true");
      document.querySelector("#cmd_disconnectRuntime").setAttribute("disabled", "true");
      document.querySelector("#cmd_showPermissionsTable").setAttribute("disabled", "true");
      document.querySelector("#cmd_takeScreenshot").setAttribute("disabled", "true");
      document.querySelector("#cmd_showRuntimeDetails").setAttribute("disabled", "true");
      document.querySelector("#cmd_play").setAttribute("disabled", "true");
      document.querySelector("#cmd_stop").setAttribute("disabled", "true");
      document.querySelector("#cmd_toggleToolbox").setAttribute("disabled", "true");
      return;
    }

    document.querySelector("#cmd_newApp").removeAttribute("disabled");
    document.querySelector("#cmd_importPackagedApp").removeAttribute("disabled");
    document.querySelector("#cmd_importHostedApp").removeAttribute("disabled");
    document.querySelector("#cmd_showProjectPanel").removeAttribute("disabled");
    document.querySelector("#cmd_showRuntimePanel").removeAttribute("disabled");

    // Action commands
    let playCmd = document.querySelector("#cmd_play");
    let stopCmd = document.querySelector("#cmd_stop");
    let debugCmd = document.querySelector("#cmd_toggleToolbox");
    let playButton = document.querySelector('#action-button-play');

    if (!AppManager.selectedProject || AppManager.connection.status != Connection.Status.CONNECTED) {
      playCmd.setAttribute("disabled", "true");
      stopCmd.setAttribute("disabled", "true");
      debugCmd.setAttribute("disabled", "true");
    } else {
      let isProjectRunning = AppManager.isProjectRunning();
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
      } else {
        if (AppManager.selectedProject.errorsCount == 0) {
          playCmd.removeAttribute("disabled");
        } else {
          playCmd.setAttribute("disabled", "true");
        }
      }
    }

    // Remove command
    let removeCmdNode = document.querySelector("#cmd_removeProject");
    if (AppManager.selectedProject) {
      removeCmdNode.removeAttribute("disabled");
    } else {
      removeCmdNode.setAttribute("disabled", "true");
    }

    // Runtime commands
    let screenshotCmd = document.querySelector("#cmd_takeScreenshot");
    let permissionsCmd = document.querySelector("#cmd_showPermissionsTable");
    let detailsCmd = document.querySelector("#cmd_showRuntimeDetails");
    let disconnectCmd = document.querySelector("#cmd_disconnectRuntime");

    let box = document.querySelector("#runtime-actions");

    let runtimePanelButton = document.querySelector("#runtime-panel-button");
    if (AppManager.connection.status == Connection.Status.CONNECTED) {
      if (AppManager.deviceFront) {
        detailsCmd.removeAttribute("disabled");
        permissionsCmd.removeAttribute("disabled");
        screenshotCmd.removeAttribute("disabled");
      }
      disconnectCmd.removeAttribute("disabled");
      runtimePanelButton.setAttribute("active", "true");
    } else {
      detailsCmd.setAttribute("disabled", "true");
      permissionsCmd.setAttribute("disabled", "true");
      screenshotCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      runtimePanelButton.removeAttribute("active");
    }

  },

  /********** TOOLBOX **********/

  onMessage: function(event) {
    // The custom toolbox sends a message to its parent
    // window.
    try {
      let json = JSON.parse(event.data);
      switch (json.name) {
        case "toolbox-close":
          this.closeToolboxUI();
          break;
      }
    } catch(e) { console.error(e); }
  },

  destroyToolbox: function() {
    if (this.toolboxPromise) {
      return this.toolboxPromise.then(toolbox => {
        toolbox.destroy();
        this.toolboxPromise = null;
      }, console.error);
    }
    return promise.resolve();
  },

  createToolbox: function() {
    this.toolboxPromise = AppManager.getTarget().then((target) => {
      return this.showToolbox(target);
    }, console.error);
    return this.busyUntil(this.toolboxPromise, "opening toolbox");
  },

  showToolbox: function(target) {
    if (this.toolboxIframe) {
      return;
    }

    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.removeAttribute("hidden");

    let iframe = document.createElement("iframe");
    document.querySelector("notificationbox").insertBefore(iframe, splitter.nextSibling);
    let host = devtools.Toolbox.HostType.CUSTOM;
    let options = { customIframe: iframe };
    this.toolboxIframe = iframe;

    let height = Services.prefs.getIntPref("devtools.toolbox.footer.height");
    iframe.height = height;

    document.querySelector("#action-button-debug").setAttribute("active", "true");

    return gDevTools.showToolbox(target, null, host, options);
  },

  closeToolboxUI: function() {
    this.resetFocus();
    Services.prefs.setIntPref("devtools.toolbox.footer.height", this.toolboxIframe.height);

    // We have to destroy the iframe, otherwise, the keybindings of webide don't work
    // properly anymore.
    this.toolboxIframe.remove();
    this.toolboxIframe = null;

    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.setAttribute("hidden", "true");
    document.querySelector("#action-button-debug").removeAttribute("active");
  },
};

let Cmds = {
  quit: function() {
    if (UI.canWindowClose()) {
      window.close();
    }
  },

  /**
   * testOptions: {       chrome mochitest support
   *   folder: nsIFile,   where to store the app
   *   index: Number,     index of the app in the template list
   *   name: String       name of the app
   * }
   */
  newApp: function(testOptions) {
    return UI.busyUntil(Task.spawn(function* () {

      // Open newapp.xul, which will feed ret.location
      let ret = {location: null, testOptions: testOptions};
      window.openDialog("chrome://webide/content/newapp.xul", "newapp", "chrome,modal", ret);
      if (!ret.location)
        return;

      // Retrieve added project
      let project = AppProjects.get(ret.location);

      // Validate project
      yield AppManager.validateProject(project);

      // Select project
      AppManager.selectedProject = project;

    }), "creating new app");
  },

  importPackagedApp: function(location) {
    return UI.busyUntil(Task.spawn(function* () {

      let directory = utils.getPackagedDirectory(window, location);

      if (!directory) {
        // User cancelled directory selection
        return;
      }

      yield UI.importAndSelectApp(directory);
    }), "importing packaged app");
  },

  importHostedApp: function(location) {
    return UI.busyUntil(Task.spawn(function* () {

      let url = utils.getHostedURL(window, location);

      if (!url) {
        return;
      }

      yield UI.importAndSelectApp(url);
    }), "importing hosted app");
  },

  showProjectPanel: function() {
    let deferred = promise.defer();

    let panelNode = document.querySelector("#project-panel");
    let panelVboxNode = document.querySelector("#project-panel > vbox");
    let anchorNode = document.querySelector("#project-panel-button > .panel-button-anchor");
    let projectsNode = document.querySelector("#project-panel-projects");

    while (projectsNode.hasChildNodes()) {
      projectsNode.firstChild.remove();
    }

    AppProjects.load().then(() => {
      let projects = AppProjects.store.object.projects;
      for (let i = 0; i < projects.length; i++) {
        let project = projects[i];
        let panelItemNode = document.createElement("toolbarbutton");
        panelItemNode.className = "panel-item";
        projectsNode.appendChild(panelItemNode);
        panelItemNode.setAttribute("label", project.name || AppManager.DEFAULT_PROJECT_NAME);
        panelItemNode.setAttribute("image", project.icon || AppManager.DEFAULT_PROJECT_ICON);
        if (!project.name || !project.icon) {
          // The result of the validation process (storing names, icons, …) is not stored in
          // the IndexedDB database when App Manager v1 is used.
          // We need to run the validation again and update the name and icon of the app.
          AppManager.validateProject(project).then(() => {
            panelItemNode.setAttribute("label", project.name);
            panelItemNode.setAttribute("image", project.icon);
          });
        }
        panelItemNode.addEventListener("click", () => {
          UI.hidePanels();
          AppManager.selectedProject = project;
        }, true);
      }

      window.setTimeout(() => {
        // Open the popup only when the projects are added.
        // Not doing it in the next tick can cause mis-calculations
        // of the size of the panel.
        function onPopupShown() {
          panelNode.removeEventListener("popupshown", onPopupShown);
          deferred.resolve();
        }
        panelNode.addEventListener("popupshown", onPopupShown);
        panelNode.openPopup(anchorNode);
        panelVboxNode.scrollTop = 0;
      }, 0);
    }, deferred.reject);


    let runtimeappsHeaderNode = document.querySelector("#panel-header-runtimeapps");
    let sortedApps = [];
    for (let [manifestURL, app] of AppManager.apps) {
      sortedApps.push(app);
    }
    sortedApps = sortedApps.sort((a, b) => {
      return a.manifest.name > b.manifest.name;
    });
    let mainProcess = AppManager.isMainProcessDebuggable();
    if (AppManager.connection.status == Connection.Status.CONNECTED &&
        (sortedApps.length > 0 || mainProcess)) {
      runtimeappsHeaderNode.removeAttribute("hidden");
    } else {
      runtimeappsHeaderNode.setAttribute("hidden", "true");
    }

    let runtimeAppsNode = document.querySelector("#project-panel-runtimeapps");
    while (runtimeAppsNode.hasChildNodes()) {
      runtimeAppsNode.firstChild.remove();
    }

    if (mainProcess) {
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item";
      panelItemNode.setAttribute("label", Strings.GetStringFromName("mainProcess_label"));
      panelItemNode.setAttribute("image", AppManager.DEFAULT_PROJECT_ICON);
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        UI.hidePanels();
        AppManager.selectedProject = {
          type: "mainProcess",
          name: Strings.GetStringFromName("mainProcess_label"),
          icon: AppManager.DEFAULT_PROJECT_ICON
        };
      }, true);
    }

    for (let i = 0; i < sortedApps.length; i++) {
      let app = sortedApps[i];
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item";
      panelItemNode.setAttribute("label", app.manifest.name);
      panelItemNode.setAttribute("image", app.iconURL);
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        UI.hidePanels();
        AppManager.selectedProject = {
          type: "runtimeApp",
          app: app.manifest,
          icon: app.iconURL,
          name: app.manifest.name
        };
      }, true);
    }

    // Build the tab list right now, so it's fast...
    this._buildProjectPanelTabs();

    // But re-list them and rebuild, in case any tabs navigated since the last
    // time they were listed.
    AppManager.listTabs().then(() => {
      this._buildProjectPanelTabs();
    });

    return deferred.promise;
  },

  _buildProjectPanelTabs: function() {
    let tabs = AppManager.tabStore.tabs;
    let tabsHeaderNode = document.querySelector("#panel-header-tabs");
    if (AppManager.connection.status == Connection.Status.CONNECTED &&
        tabs.length > 0) {
      tabsHeaderNode.removeAttribute("hidden");
    } else {
      tabsHeaderNode.setAttribute("hidden", "true");
    }

    let tabsNode = document.querySelector("#project-panel-tabs");
    while (tabsNode.hasChildNodes()) {
      tabsNode.firstChild.remove();
    }

    for (let i = 0; i < tabs.length; i++) {
      let tab = tabs[i];
      let url = new URL(tab.url);
      // Wanted to use nsIFaviconService here, but it only works for visited
      // tabs, so that's no help for any remote tabs.  Maybe some favicon wizard
      // knows how to get high-res favicons easily, or we could offer actor
      // support for this (bug 1061654).
      tab.favicon = url.origin + "/favicon.ico";
      tab.name = tab.title || Strings.GetStringFromName("project_tab_loading");
      if (url.protocol.startsWith("http")) {
        tab.name = url.hostname + ": " + tab.name;
      }
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item";
      panelItemNode.setAttribute("label", tab.name);
      panelItemNode.setAttribute("image", tab.favicon);
      tabsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        UI.hidePanels();
        AppManager.selectedProject = {
          type: "tab",
          app: tab,
          icon: tab.favicon,
          location: tab.url,
          name: tab.name
        };
      }, true);
    }
  },

  showRuntimePanel: function() {
    RuntimeScanners.scan();

    let panel = document.querySelector("#runtime-panel");
    let anchor = document.querySelector("#runtime-panel-button > .panel-button-anchor");

    let deferred = promise.defer();
    function onPopupShown() {
      panel.removeEventListener("popupshown", onPopupShown);
      deferred.resolve();
    }
    panel.addEventListener("popupshown", onPopupShown);

    panel.openPopup(anchor);
    return deferred.promise;
  },

  disconnectRuntime: function() {
    return UI.busyUntil(AppManager.disconnectRuntime(), "disconnecting from runtime");
  },

  takeScreenshot: function() {
    return UI.busyUntil(AppManager.deviceFront.screenshotToDataURL().then(longstr => {
       return longstr.string().then(dataURL => {
         longstr.release().then(null, console.error);
         UI.openInBrowser(dataURL);
       });
    }), "taking screenshot");
  },

  showPermissionsTable: function() {
    UI.selectDeckPanel("permissionstable");
  },

  showRuntimeDetails: function() {
    UI.selectDeckPanel("runtimedetails");
  },

  showMonitor: function() {
    UI.selectDeckPanel("monitor");
  },

  play: function() {
    let busy;
    switch(AppManager.selectedProject.type) {
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
    if (UI.toolboxIframe) {
      UI.destroyToolbox();
      return promise.resolve();
    } else {
      return UI.createToolbox();
    }
  },

  removeProject: function() {
    return AppManager.removeSelectedProject();
  },

  toggleEditors: function() {
    let isNowEnabled = !UI.isProjectEditorEnabled();
    Services.prefs.setBoolPref("devtools.webide.showProjectEditor", isNowEnabled);
    if (!isNowEnabled) {
      UI.destroyProjectEditor();
    }
    UI.openProject();
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
};
