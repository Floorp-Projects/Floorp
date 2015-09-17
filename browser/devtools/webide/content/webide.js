/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {Toolbox} = require("devtools/framework/toolbox");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {AppProjects} = require("devtools/app-manager/app-projects");
const {Connection} = require("devtools/client/connection-manager");
const {AppManager} = require("devtools/webide/app-manager");
const EventEmitter = require("devtools/toolkit/event-emitter");
const promise = require("promise");
const ProjectEditor = require("projecteditor/projecteditor");
const {GetAvailableAddons} = require("devtools/webide/addons");
const {getJSON} = require("devtools/shared/getjson");
const utils = require("devtools/webide/utils");
const Telemetry = require("devtools/shared/telemetry");
const {RuntimeScanners} = require("devtools/webide/runtimes");
const {showDoorhanger} = require("devtools/shared/doorhanger");
const ProjectList = require("devtools/webide/project-list");
const {Simulators} = require("devtools/webide/simulators");
const RuntimeList = require("devtools/webide/runtime-list");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

const HTML = "http://www.w3.org/1999/xhtml";
const HELP_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Troubleshooting";

const MAX_ZOOM = 1.4;
const MIN_ZOOM = 0.6;

const MS_PER_DAY = 86400000;

// Download remote resources early
getJSON("devtools.webide.addonsURL", true);
getJSON("devtools.webide.templatesURL", true);
getJSON("devtools.devices.url", true);

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
  UI.destroy();
});

var projectList;
var runtimeList;

var UI = {
  init: function() {
    this._telemetry = new Telemetry();
    this._telemetry.toolOpened("webide");

    AppManager.init();

    this.onMessage = this.onMessage.bind(this);
    window.addEventListener("message", this.onMessage);

    this.appManagerUpdate = this.appManagerUpdate.bind(this);
    AppManager.on("app-manager-update", this.appManagerUpdate);

    projectList = new ProjectList(window, window);
    if (projectList.sidebarsEnabled) {
      ProjectPanel.toggleSidebar();

      // TODO: Remove if/when dropdown layout is removed.
      let toolbarNode = document.querySelector("#main-toolbar");
      toolbarNode.classList.add("sidebar-layout");
      let projectNode = document.querySelector("#project-panel-button");
      projectNode.setAttribute("hidden", "true");
      let runtimeNode = document.querySelector("#runtime-panel-button");
      runtimeNode.setAttribute("hidden", "true");
      let openAppNode = document.querySelector("#menuitem-show_projectPanel");
      openAppNode.setAttribute("hidden", "true");
    }
    runtimeList = new RuntimeList(window, window);
    if (runtimeList.sidebarsEnabled) {
      Cmds.showRuntimePanel();
    } else {
      runtimeList.update();
    }

    this.updateCommands();

    this.onfocus = this.onfocus.bind(this);
    window.addEventListener("focus", this.onfocus, true);

    AppProjects.load().then(() => {
      this.autoSelectProject();
      if (!projectList.sidebarsEnabled) {
        projectList.update();
      }
    }, e => {
      console.error(e);
      this.reportError("error_appProjectsLoadFailed");
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

    if (Services.prefs.getBoolPref("devtools.webide.widget.autoinstall") &&
        !Services.prefs.getBoolPref("devtools.webide.widget.enabled")) {
      Services.prefs.setBoolPref("devtools.webide.widget.enabled", true);
      gDevToolsBrowser.moveWebIDEWidgetInNavbar();
    }

    this.setupDeck();

    this.contentViewer = window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .contentViewer;
    this.contentViewer.fullZoom = Services.prefs.getCharPref("devtools.webide.zoom");

    gDevToolsBrowser.isWebIDEInitialized.resolve();

    this.configureSimulator = this.configureSimulator.bind(this);
    Simulators.on("configure", this.configureSimulator);
  },

  destroy: function() {
    window.removeEventListener("focus", this.onfocus, true);
    AppManager.off("app-manager-update", this.appManagerUpdate);
    AppManager.destroy();
    Simulators.off("configure", this.configureSimulator);
    projectList.destroy();
    runtimeList.destroy();
    window.removeEventListener("message", this.onMessage);
    this.updateConnectionTelemetry();
    this._telemetry.toolClosed("webide");
    this._telemetry.toolClosed("webideProjectEditor");
    this._telemetry.destroy();
  },

  canCloseProject: function() {
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
      AppManager.validateAndUpdateProject(AppManager.selectedProject);
    }

    // Hook to display promotional Developer Edition doorhanger. Only displayed once.
    // Hooked into the `onfocus` event because sometimes does not work
    // when run at the end of `init`. ¯\(°_o)/¯
    showDoorhanger({ window, type: "deveditionpromo", anchor: document.querySelector("#deck") });
  },

  appManagerUpdate: function(event, what, details) {
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
      case "before-project":
        if (!this.canCloseProject())  {
          details.cancel();
        }
        break;
      case "project":
        this._updatePromise = Task.spawn(function*() {
          UI.updateTitle();
          yield UI.destroyToolbox();
          UI.updateCommands();
          UI.updateProjectButton();
          UI.openProject();
          yield UI.autoStartProject();
          UI.autoOpenToolbox();
          UI.saveLastSelectedProject();
          UI.updateRemoveProjectButton();
        });
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
        this.updateProjectButton();
        this.updateProjectEditorHeader();
        break;
      case "install-progress":
        this.updateProgress(Math.round(100 * details.bytesSent / details.totalBytes));
        break;
      case "runtime-targets":
        this.autoSelectProject();
        break;
      case "pre-package":
        this.prePackageLog(details);
        break;
    };
    this._updatePromise = promise.resolve();
  },

  configureSimulator: function(event, simulator) {
    UI.selectDeckPanel("simulator");
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

  // TODO: remove hidePanel when project layout is complete - Bug 1079347
  hidePanels: function() {
    let panels = document.querySelectorAll("panel");
    for (let p of panels) {
      // Sometimes in tests, p.hidePopup is not defined - Bug 1151796.
      p.hidePopup && p.hidePopup();
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
    win.classList.add("busy");
    win.classList.add("busy-undetermined");
    this.updateCommands();
    this.update("busy");
  },

  unbusy: function() {
    let win = document.querySelector("window");
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
    let busy = this.busyUntil(promise, operationDescription);
    let win = document.querySelector("window");
    let progress = document.querySelector("#action-busy-determined");
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
      let operationCanceled = e && e.canceled;
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

  /********** COMMANDS **********/

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
    let playCmd = document.querySelector("#cmd_play");
    let stopCmd = document.querySelector("#cmd_stop");
    let debugCmd = document.querySelector("#cmd_toggleToolbox");
    let playButton = document.querySelector('#action-button-play');
    let projectPanelCmd = document.querySelector("#cmd_showProjectPanel");

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
        if (AppManager.selectedProject.errorsCount == 0 &&
            AppManager.runtimeCanHandleApps()) {
          playCmd.removeAttribute("disabled");
        } else {
          playCmd.setAttribute("disabled", "true");
        }
      }
    }

    // Runtime commands
    let monitorCmd = document.querySelector("#cmd_showMonitor");
    let screenshotCmd = document.querySelector("#cmd_takeScreenshot");
    let permissionsCmd = document.querySelector("#cmd_showPermissionsTable");
    let detailsCmd = document.querySelector("#cmd_showRuntimeDetails");
    let disconnectCmd = document.querySelector("#cmd_disconnectRuntime");
    let devicePrefsCmd = document.querySelector("#cmd_showDevicePrefs");
    let settingsCmd = document.querySelector("#cmd_showSettings");

    if (AppManager.connected) {
      if (AppManager.deviceFront) {
        monitorCmd.removeAttribute("disabled");
        detailsCmd.removeAttribute("disabled");
        permissionsCmd.removeAttribute("disabled");
        screenshotCmd.removeAttribute("disabled");
      }
      if (AppManager.preferenceFront) {
        devicePrefsCmd.removeAttribute("disabled");
      }
      if (AppManager.settingsFront) {
        settingsCmd.removeAttribute("disabled");
      }
      disconnectCmd.removeAttribute("disabled");
    } else {
      monitorCmd.setAttribute("disabled", "true");
      detailsCmd.setAttribute("disabled", "true");
      permissionsCmd.setAttribute("disabled", "true");
      screenshotCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      devicePrefsCmd.setAttribute("disabled", "true");
      settingsCmd.setAttribute("disabled", "true");
    }

    let runtimePanelButton = document.querySelector("#runtime-panel-button");

    if (AppManager.connected) {
      runtimePanelButton.setAttribute("active", "true");
      if (projectList.sidebarsEnabled) {
        runtimePanelButton.removeAttribute("hidden");
      }
    } else {
      runtimePanelButton.removeAttribute("active");
      if (projectList.sidebarsEnabled) {
        runtimePanelButton.setAttribute("hidden", "true");
      }
    }

    projectPanelCmd.removeAttribute("disabled");
  },

  updateRemoveProjectButton: function() {
    // Remove command
    let removeCmdNode = document.querySelector("#cmd_removeProject");
    if (AppManager.selectedProject) {
      removeCmdNode.removeAttribute("disabled");
    } else {
      removeCmdNode.setAttribute("disabled", "true");
    }
  },

  /********** RUNTIME **********/

  get lastConnectedRuntime() {
    return Services.prefs.getCharPref("devtools.webide.lastConnectedRuntime");
  },

  set lastConnectedRuntime(runtime) {
    Services.prefs.setCharPref("devtools.webide.lastConnectedRuntime", runtime);
  },

  autoConnectRuntime: function () {
    // Automatically reconnect to the previously selected runtime,
    // if available and has an ID and feature is enabled
    if (AppManager.selectedRuntime ||
        !Services.prefs.getBoolPref("devtools.webide.autoConnectRuntime") ||
        !this.lastConnectedRuntime) {
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
          // Only want one auto-connect attempt, so clear last runtime value
          this.lastConnectedRuntime = "";
          this.connectToRuntime(runtime);
        }
      }
    }
  },

  connectToRuntime: function(runtime) {
    let name = runtime.name;
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
  },

  /********** ACTIONS **********/

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

    if (!projectList.sidebarsEnabled) {
      if (!project) {
        buttonNode.classList.add("no-project");
        labelNode.setAttribute("value", Strings.GetStringFromName("projectButton_label"));
        imageNode.removeAttribute("src");
      } else {
        buttonNode.classList.remove("no-project");
        labelNode.setAttribute("value", project.name);
        imageNode.setAttribute("src", project.icon);
      }
    }
  },

  // ProjectEditor & details screen

  destroyProjectEditor: function() {
    if (this.projecteditor) {
      this.projecteditor.destroy();
      this.projecteditor = null;
    }
  },

  /**
   * Called when selecting or deselecting the project editor panel.
   */
  onChangeProjectEditorSelected: function() {
    if (this.projecteditor) {
      let panel = document.querySelector("#deck").selectedPanel;
      if (panel && panel.id == "deck-panel-projecteditor") {
        this.projecteditor.menuEnabled = true;
        this._telemetry.toolOpened("webideProjectEditor");
      } else {
        this.projecteditor.menuEnabled = false;
        this._telemetry.toolClosed("webideProjectEditor");
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
    this.projecteditor.on("onEditorSave", () => {
      AppManager.validateAndUpdateProject(AppManager.selectedProject);
      this._telemetry.actionOccurred("webideProjectEditorSave");
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

    this.getProjectEditor().then(() => {
      this.updateProjectEditorHeader();
    }, console.error);

    this.selectDeckPanel("projecteditor");
  },

  autoStartProject: Task.async(function*() {
    let project = AppManager.selectedProject;

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
      yield UI.busyUntil(AppManager.launchRuntimeApp(), "running app");
    }
  }),

  autoOpenToolbox: Task.async(function*() {
    let project = AppManager.selectedProject;

    if (!project) {
      return;
    }
    if (!(project.type == "runtimeApp" ||
          project.type == "mainProcess" ||
          project.type == "tab")) {
      return; // For something that is not an editable app, we're done.
    }

    yield UI.createToolbox();
  }),

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

    // Select project
    AppManager.selectedProject = project;

    this._telemetry.actionOccurred("webideImportProject");
  }),

  // Remember the last selected project on the runtime
  saveLastSelectedProject: function() {
    let shouldRestore = Services.prefs.getBoolPref("devtools.webide.restoreLastProject");
    if (!shouldRestore) {
      return;
    }

    // Ignore unselection of project on runtime disconnection
    if (!AppManager.connected) {
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
    if (!AppManager.connected) {
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
    let deck = document.querySelector("#deck");
    if (deck.selectedPanel && deck.selectedPanel.id === "deck-panel-" + id) {
      // This panel is already displayed.
      return;
    }
    this.hidePanels();
    this.resetFocus();
    let panel = deck.querySelector("#deck-panel-" + id);
    let lazysrc = panel.getAttribute("lazysrc");
    if (lazysrc) {
      panel.removeAttribute("lazysrc");
      panel.setAttribute("src", lazysrc);
    }
    deck.selectedPanel = panel;
    this.onChangeProjectEditorSelected();
    this.updateToolboxFullscreenState();
  },

  resetDeck: function() {
    this.resetFocus();
    let deck = document.querySelector("#deck");
    deck.selectedPanel = null;
    this.onChangeProjectEditorSelected();
  },

  buildIDToDate(buildID) {
    let fields = buildID.match(/(\d{4})(\d{2})(\d{2})/);
    // Date expects 0 - 11 for months
    return new Date(fields[1], Number.parseInt(fields[2]) - 1, fields[3]);
  },

  checkRuntimeVersion: Task.async(function* () {
    if (AppManager.connected && AppManager.deviceFront) {
      let desc = yield AppManager.deviceFront.getDescription();
      // Compare device and firefox build IDs
      // and only compare by day (strip hours/minutes) to prevent
      // warning against builds of the same day.
      let deviceID = desc.appbuildid.substr(0, 8);
      let localID = Services.appinfo.appBuildID.substr(0, 8);
      let deviceDate = this.buildIDToDate(deviceID);
      let localDate = this.buildIDToDate(localID);
      // Allow device to be newer by up to a week.  This accommodates those with
      // local device builds, since their devices will almost always be newer
      // than the client.
      if (deviceDate - localDate > 7 * MS_PER_DAY) {
        this.reportError("error_runtimeVersionTooRecent", deviceID, localID);
      }
    }
  }),

  /********** TOOLBOX **********/

  onMessage: function(event) {
    // The custom toolbox sends a message to its parent
    // window.
    try {
      let json = JSON.parse(event.data);
      switch (json.name) {
        case "toolbox-close":
          // There are many ways to close a toolbox:
          // * Close button inside the toolbox
          // * Toggle toolbox wrench in WebIDE
          // * Disconnect the current runtime gracefully
          // * Yank cord out of device
          // We can't know for sure which one was used here, so reset the
          // |toolboxPromise| since someone must be destroying it to reach here,
          // and call our own close method.
          if (this.toolboxIframe && this.toolboxIframe.uid == json.uid) {
            this.toolboxPromise = null;
            this._closeToolboxUI();
          }
          break;
      }
    } catch(e) { console.error(e); }
  },

  destroyToolbox: function() {
    // Only have a live toolbox if |this.toolboxPromise| exists
    if (this.toolboxPromise) {
      let toolboxPromise = this.toolboxPromise;
      this.toolboxPromise = null;
      return toolboxPromise.then(toolbox => {
        return toolbox.destroy();
      }).then(null, console.error)
        .then(() => this._closeToolboxUI())
        .then(null, console.error);
    }
    return promise.resolve();
  },

  createToolbox: function() {
    // If |this.toolboxPromise| exists, there is already a live toolbox
    if (this.toolboxPromise) {
      return this.toolboxPromise;
    }
    this.toolboxPromise = AppManager.getTarget().then((target) => {
      return this._showToolbox(target);
    }, console.error);
    return this.busyUntil(this.toolboxPromise, "opening toolbox");
  },

  _showToolbox: function(target) {
    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.removeAttribute("hidden");

    let iframe = document.createElement("iframe");
    iframe.id = "toolbox";

    // Compute a uid on the iframe in order to identify toolbox iframe
    // when receiving toolbox-close event
    iframe.uid = new Date().getTime();

    document.querySelector("notificationbox").insertBefore(iframe, splitter.nextSibling);
    let host = Toolbox.HostType.CUSTOM;
    let options = { customIframe: iframe, zoom: false, uid: iframe.uid };
    this.toolboxIframe = iframe;

    let height = Services.prefs.getIntPref("devtools.toolbox.footer.height");
    iframe.height = height;

    document.querySelector("#action-button-debug").setAttribute("active", "true");

    this.updateToolboxFullscreenState();
    return gDevTools.showToolbox(target, null, host, options);
  },

  updateToolboxFullscreenState: function() {
    if (projectList.sidebarsEnabled) {
      return;
    }

    let panel = document.querySelector("#deck").selectedPanel;
    let nbox = document.querySelector("#notificationbox");
    if (panel && panel.id == "deck-panel-details" &&
        AppManager.selectedProject &&
        AppManager.selectedProject.type != "packaged" &&
        this.toolboxIframe) {
      nbox.setAttribute("toolboxfullscreen", "true");
    } else {
      nbox.removeAttribute("toolboxfullscreen");
    }
  },

  _closeToolboxUI: function() {
    if (!this.toolboxIframe) {
      return;
    }

    this.resetFocus();
    Services.prefs.setIntPref("devtools.toolbox.footer.height", this.toolboxIframe.height);

    // We have to destroy the iframe, otherwise, the keybindings of webide don't work
    // properly anymore.
    this.toolboxIframe.remove();
    this.toolboxIframe = null;

    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.setAttribute("hidden", "true");
    document.querySelector("#action-button-debug").removeAttribute("active");
    this.updateToolboxFullscreenState();
  },

  prePackageLog: function (msg) {
    if (msg == "start") {
      UI.selectDeckPanel("logs");
    }
  }
};

EventEmitter.decorate(UI);

var Cmds = {
  quit: function() {
    if (UI.canCloseProject()) {
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
    projectList.newApp(testOptions);
  },

  importPackagedApp: function(location) {
    projectList.importPackagedApp(location);
  },

  importHostedApp: function(location) {
    projectList.importHostedApp(location);
  },

  showProjectPanel: function() {
    if (projectList.sidebarsEnabled) {
      ProjectPanel.toggleSidebar();
    } else {
      ProjectPanel.showPopup();
    }

    // TODO: Remove this check if/when we remove the dropdown view.
    if (!projectList.sidebarsEnabled && AppManager.connected) {
      projectList.refreshTabs();
    }

    return promise.resolve();
  },

  showRuntimePanel: function() {
    RuntimeScanners.scan();

    if (runtimeList.sidebarsEnabled) {
      RuntimePanel.toggleSidebar();
    } else {
      RuntimePanel.showPopup();
    }
  },

  disconnectRuntime: function() {
    let disconnecting = Task.spawn(function*() {
      yield UI.destroyToolbox();
      yield AppManager.disconnectRuntime();
    });
    return UI.busyUntil(disconnecting, "disconnecting from runtime");
  },

  takeScreenshot: function() {
    runtimeList.takeScreenshot();
  },

  showPermissionsTable: function() {
    UI.selectDeckPanel("permissionstable");
  },

  showRuntimeDetails: function() {
    UI.selectDeckPanel("runtimedetails");
  },

  showDevicePrefs: function() {
    UI.selectDeckPanel("devicepreferences");
  },

  showSettings: function() {
    UI.selectDeckPanel("devicesettings");
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
    AppManager.removeSelectedProject();
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
  },
};
