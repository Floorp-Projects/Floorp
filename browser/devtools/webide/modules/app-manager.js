/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {Simulator} = Cu.import("resource://gre/modules/devtools/Simulator.jsm");
const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js");
const {TextEncoder, OS}  = Cu.import("resource://gre/modules/osfile.jsm", {});
const {AppProjects} = require("devtools/app-manager/app-projects");
const TabStore = require("devtools/webide/tab-store");
const {AppValidator} = require("devtools/app-manager/app-validator");
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const {AppActorFront} = require("devtools/app-actor-front");
const {getDeviceFront} = require("devtools/server/actors/device");
const {getPreferenceFront} = require("devtools/server/actors/preference");
const {setTimeout} = require("sdk/timers");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {USBRuntime, WiFiRuntime, SimulatorRuntime,
       gLocalRuntime, gRemoteRuntime} = require("devtools/webide/runtimes");
const discovery = require("devtools/toolkit/discovery/discovery");
const {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const Telemetry = require("devtools/shared/telemetry");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

const WIFI_SCANNING_PREF = "devtools.remote.wifi.scan";

exports.AppManager = AppManager = {

  // FIXME: will break when devtools/app-manager will be removed:
  DEFAULT_PROJECT_ICON: "chrome://browser/skin/devtools/app-manager/default-app-icon.png",
  DEFAULT_PROJECT_NAME: "--",

  init: function() {
    let host = Services.prefs.getCharPref("devtools.debugger.remote-host");
    let port = Services.prefs.getIntPref("devtools.debugger.remote-port");

    this.connection = ConnectionManager.createConnection("localhost", port);
    this.onConnectionChanged = this.onConnectionChanged.bind(this);
    this.connection.on(Connection.Events.STATUS_CHANGED, this.onConnectionChanged);

    this.tabStore = new TabStore(this.connection);
    this.onTabNavigate = this.onTabNavigate.bind(this);
    this.onTabClosed = this.onTabClosed.bind(this);
    this.tabStore.on("navigate", this.onTabNavigate);
    this.tabStore.on("closed", this.onTabClosed);

    this.runtimeList = {
      usb: [],
      wifi: [],
      simulator: [],
      custom: [gRemoteRuntime]
    };
    if (Services.prefs.getBoolPref("devtools.webide.enableLocalRuntime")) {
      this.runtimeList.custom.push(gLocalRuntime);
    }
    this.trackUSBRuntimes();
    this.trackWiFiRuntimes();
    this.trackSimulatorRuntimes();

    this.onInstallProgress = this.onInstallProgress.bind(this);

    this.observe = this.observe.bind(this);
    Services.prefs.addObserver(WIFI_SCANNING_PREF, this, false);

    this._telemetry = new Telemetry();
  },

  uninit: function() {
    this.selectedProject = null;
    this.selectedRuntime = null;
    this.untrackUSBRuntimes();
    this.untrackWiFiRuntimes();
    this.untrackSimulatorRuntimes();
    this.runtimeList = null;
    this.tabStore.off("navigate", this.onTabNavigate);
    this.tabStore.off("closed", this.onTabClosed);
    this.tabStore.destroy();
    this.tabStore = null;
    this.connection.off(Connection.Events.STATUS_CHANGED, this.onConnectionChanged);
    this._listTabsResponse = null;
    this.connection.disconnect();
    this.connection = null;
    Services.prefs.removeObserver(WIFI_SCANNING_PREF, this);
  },

  observe: function(subject, topic, data) {
    if (data !== WIFI_SCANNING_PREF) {
      return;
    }
    // Cycle WiFi tracking to reflect the new value
    this.untrackWiFiRuntimes();
    this.trackWiFiRuntimes();
    this._updateWiFiRuntimes();
  },

  update: function(what, details) {
    // Anything we want to forward to the UI
    this.emit("app-manager-update", what, details);
  },

  reportError: function(l10nProperty, ...l10nArgs) {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.UI.reportError(l10nProperty, ...l10nArgs);
    } else {
      let text;
      if (l10nArgs.length > 0) {
        text = Strings.formatStringFromName(l10nProperty, l10nArgs, l10nArgs.length);
      } else {
        text = Strings.GetStringFromName(l10nProperty);
      }
      console.error(text);
    }
  },

  onConnectionChanged: function() {
    if (this.connection.status == Connection.Status.DISCONNECTED) {
      this.selectedRuntime = null;
    }

    if (this.connection.status != Connection.Status.CONNECTED) {
      console.log("Connection status changed: " + this.connection.status);
      if (this._appsFront) {
        this._appsFront.off("install-progress", this.onInstallProgress);
        this._appsFront.unwatchApps();
        this._appsFront = null;
      }
      this._listTabsResponse = null;
    } else {
      this.connection.client.listTabs((response) => {
        let front = new AppActorFront(this.connection.client,
                                      response);
        front.on("install-progress", this.onInstallProgress);
        front.watchApps(() => this.checkIfProjectIsRunning())
             .then(() => front.fetchIcons())
             .then(() => {
               this._appsFront = front;
               this.checkIfProjectIsRunning();
               this.update("runtime-apps-found");
             });
        this._listTabsResponse = response;
        this.update("list-tabs-response");
      });
    }

    this.update("connection");
  },

  get apps() {
    if (this._appsFront) {
      return this._appsFront.apps;
    } else {
      return new Map();
    }
  },

  onInstallProgress: function(event, details) {
    this.update("install-progress", details);
  },

  isProjectRunning: function() {
    if (this.selectedProject.type == "mainProcess" ||
        this.selectedProject.type == "tab") {
      return true;
    }

    let app = this._getProjectFront(this.selectedProject);
    return app && app.running;
  },

  checkIfProjectIsRunning: function() {
    if (this.selectedProject) {
      if (this.isProjectRunning()) {
        this.update("project-is-running");
      } else {
        this.update("project-is-not-running");
      }
    }
  },

  listTabs: function() {
    return this.tabStore.listTabs();
  },

  // TODO: Merge this into TabProject as part of project-agnostic work
  onTabNavigate: function() {
    if (this.selectedProject.type !== "tab") {
      return;
    }
    let tab = this.selectedProject.app = this.tabStore.selectedTab;
    let uri = NetUtil.newURI(tab.url);
    // Wanted to use nsIFaviconService here, but it only works for visited
    // tabs, so that's no help for any remote tabs.  Maybe some favicon wizard
    // knows how to get high-res favicons easily, or we could offer actor
    // support for this (bug 1061654).
    tab.favicon = uri.prePath + "/favicon.ico";
    tab.name = tab.title || Strings.GetStringFromName("project_tab_loading");
    if (uri.scheme.startsWith("http")) {
      tab.name = uri.host + ": " + tab.name;
    }
    this.selectedProject.location = tab.url;
    this.selectedProject.name = tab.name;
    this.selectedProject.icon = tab.favicon;
    this.update("project-validated");
  },

  onTabClosed: function() {
    if (this.selectedProject.type !== "tab") {
      return;
    }
    this.selectedProject = null;
  },

  reloadTab: function() {
    if (this.selectedProject && this.selectedProject.type != "tab") {
      return promise.reject("tried to reload non-tab project");
    }
    return this.getTarget().then(target => {
      target.activeTab.reload();
    }, console.error.bind(console));
  },

  getTarget: function() {
    if (this.selectedProject.type == "mainProcess") {
      return devtools.TargetFactory.forRemoteTab({
        form: this._listTabsResponse,
        client: this.connection.client,
        chrome: true
      });
    }

    if (this.selectedProject.type == "tab") {
      return this.tabStore.getTargetForTab();
    }

    let app = this._getProjectFront(this.selectedProject);
    if (!app) {
      return promise.reject("Can't find app front for selected project");
    }

    return Task.spawn(function* () {
      // Once we asked the app to launch, the app isn't necessary completely loaded.
      // launch request only ask the app to launch and immediatly returns.
      // We have to keep trying to get app tab actors required to create its target.

      for (let i = 0; i < 10; i++) {
        try {
          return yield app.getTarget();
        } catch(e) {}
        let deferred = promise.defer();
        setTimeout(deferred.resolve, 500);
        yield deferred.promise;
      }

      AppManager.reportError("error_cantConnectToApp", app.manifest.manifestURL);
      throw new Error("can't connect to app");
    });
  },

  getProjectManifestURL: function(project) {
    let manifest = null;
    if (project.type == "runtimeApp") {
      manifest = project.app.manifestURL;
    }

    if (project.type == "hosted") {
      manifest = project.location;
    }

    if (project.type == "packaged" && project.packagedAppOrigin) {
      manifest = "app://" + project.packagedAppOrigin + "/manifest.webapp";
    }

    return manifest;
  },

  _getProjectFront: function(project) {
    let manifest = this.getProjectManifestURL(project);
    if (manifest && this._appsFront) {
      return this._appsFront.apps.get(manifest);
    }
    return null;
  },

  _selectedProject: null,
  set selectedProject(value) {
    // A regular comparison still sees a difference when equal in some cases
    if (JSON.stringify(this._selectedProject) !==
        JSON.stringify(value)) {
      this._selectedProject = value;

      // Clear out tab store's selected state, if any
      this.tabStore.selectedTab = null;

      if (this.selectedProject) {
        if (this.selectedProject.type == "packaged" ||
            this.selectedProject.type == "hosted") {
          this.validateProject(this.selectedProject);
        }
        if (this.selectedProject.type == "tab") {
          this.tabStore.selectedTab = this.selectedProject.app;
        }
      }

      this.update("project");

      this.checkIfProjectIsRunning();
    }
  },
  get selectedProject() {
    return this._selectedProject;
  },

  removeSelectedProject: function() {
    let location = this.selectedProject.location;
    AppManager.selectedProject = null;
    return AppProjects.remove(location);
  },

  _selectedRuntime: null,
  set selectedRuntime(value) {
    this._selectedRuntime = value;
    if (!value && this.selectedProject &&
        (this.selectedProject.type == "mainProcess" ||
         this.selectedProject.type == "runtimeApp" ||
         this.selectedProject.type == "tab")) {
      this.selectedProject = null;
    }
    this.update("runtime");
  },

  get selectedRuntime() {
    return this._selectedRuntime;
  },

  connectToRuntime: function(runtime) {

    if (this.connection.status == Connection.Status.CONNECTED &&
        this.selectedRuntime === runtime) {
      // Already connected
      return promise.resolve();
    }

    let deferred = promise.defer();

    this.disconnectRuntime().then(() => {
      this.selectedRuntime = runtime;

      let onConnectedOrDisconnected = () => {
        this.connection.off(Connection.Events.CONNECTED, onConnectedOrDisconnected);
        this.connection.off(Connection.Events.DISCONNECTED, onConnectedOrDisconnected);
        if (this.connection.status == Connection.Status.CONNECTED) {
          deferred.resolve();
        } else {
          deferred.reject();
        }
      }
      this.connection.on(Connection.Events.CONNECTED, onConnectedOrDisconnected);
      this.connection.on(Connection.Events.DISCONNECTED, onConnectedOrDisconnected);
      try {
        this.selectedRuntime.connect(this.connection).then(
          () => {},
          deferred.reject.bind(deferred));
      } catch(e) {
        console.error(e);
        deferred.reject();
      }
    }, deferred.reject);

    // Record connection result in telemetry
    let logResult = result => {
      this._telemetry.log("DEVTOOLS_WEBIDE_CONNECTION_RESULT", result);
      if (runtime.type) {
        this._telemetry.log("DEVTOOLS_WEBIDE_" + runtime.type +
                            "_CONNECTION_RESULT", result);
      }
    };
    deferred.promise.then(() => logResult(true), () => logResult(false));

    // If successful, record connection time in telemetry
    deferred.promise.then(() => {
      const timerId = "DEVTOOLS_WEBIDE_CONNECTION_TIME_SECONDS";
      this._telemetry.startTimer(timerId);
      this.connection.once(Connection.Events.STATUS_CHANGED, () => {
        this._telemetry.stopTimer(timerId);
      });
    });

    return deferred.promise;
  },

  isMainProcessDebuggable: function() {
    return this._listTabsResponse &&
           this._listTabsResponse.consoleActor;
  },

  get deviceFront() {
    if (!this._listTabsResponse) {
      return null;
    }
    return getDeviceFront(this.connection.client, this._listTabsResponse);
  },

  get preferenceFront() {
    if (!this._listTabsResponse) {
      return null;
    }
    return getPreferenceFront(this.connection.client, this._listTabsResponse);
  },

  disconnectRuntime: function() {
    if (this.connection.status != Connection.Status.CONNECTED) {
      return promise.resolve();
    }
    let deferred = promise.defer();
    this.connection.once(Connection.Events.DISCONNECTED, () => deferred.resolve());
    this.connection.disconnect();
    return deferred.promise;
  },

  launchRuntimeApp: function() {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return promise.reject("attempting to launch a non-runtime app");
    }
    let app = this._getProjectFront(this.selectedProject);
    return app.launch();
  },

  launchOrReloadRuntimeApp: function() {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return promise.reject("attempting to launch / reload a non-runtime app");
    }
    let app = this._getProjectFront(this.selectedProject);
    if (!app.running) {
      return app.launch();
    } else {
      return app.reload();
    }
  },

  installAndRunProject: function() {
    let project = this.selectedProject;

    if (!project || (project.type != "packaged" && project.type != "hosted")) {
      console.error("Can't install project. Unknown type of project.");
      return promise.reject("Can't install");
    }

    if (!this._listTabsResponse) {
      this.reportError("error_cantInstallNotFullyConnected");
      return promise.reject("Can't install");
    }

    return Task.spawn(function* () {
      let self = AppManager;

      yield self.validateProject(project);

      if (project.errorsCount > 0) {
        self.reportError("error_cantInstallValidationErrors");
        return;
      }

      let installPromise;

      if (project.type != "packaged" && project.type != "hosted") {
        return promise.reject("Don't know how to install project");
      }

      let response;
      if (project.type == "packaged") {
        response = yield self._appsFront.installPackaged(project.location,
                                                             project.packagedAppOrigin);

        // If the packaged app specified a custom origin override,
        // we need to update the local project origin
        project.packagedAppOrigin = response.appId;
        // And ensure the indexed db on disk is also updated
        AppProjects.update(project);
      }

      if (project.type == "hosted") {
        let manifestURLObject = Services.io.newURI(project.location, null, null);
        let origin = Services.io.newURI(manifestURLObject.prePath, null, null);
        let appId = origin.host;
        let metadata = {
          origin: origin.spec,
          manifestURL: project.location
        };
        response = yield self._appsFront.installHosted(appId,
                                            metadata,
                                            project.manifest);
      }

      let {app} = response;
      if (!app.running) {
        let deferred = promise.defer();
        self.on("app-manager-update", function onUpdate(event, what) {
          if (what == "project-is-running") {
            self.off("app-manager-update", onUpdate);
            deferred.resolve();
          }
        });
        yield app.launch();
        yield deferred.promise;
      } else {
        yield app.reload();
      }
    });
  },

  stopRunningApp: function() {
    let app = this._getProjectFront(this.selectedProject);
    return app.close();
  },

  /* PROJECT VALIDATION */

  validateProject: function(project) {
    if (!project) {
      return promise.reject();
    }

    return Task.spawn(function* () {

      let validation = new AppValidator(project);
      yield validation.validate();

      if (validation.manifest) {
        let manifest = validation.manifest;
        let iconPath;
        if (manifest.icons) {
          let size = Object.keys(manifest.icons).sort(function(a, b) b - a)[0];
          if (size) {
            iconPath = manifest.icons[size];
          }
        }
        if (!iconPath) {
          project.icon = AppManager.DEFAULT_PROJECT_ICON;
        } else {
          if (project.type == "hosted") {
            let manifestURL = Services.io.newURI(project.location, null, null);
            let origin = Services.io.newURI(manifestURL.prePath, null, null);
            project.icon = Services.io.newURI(iconPath, null, origin).spec;
          } else if (project.type == "packaged") {
            let projectFolder = FileUtils.File(project.location);
            let folderURI = Services.io.newFileURI(projectFolder).spec;
            project.icon = folderURI + iconPath.replace(/^\/|\\/, "");
          }
        }
        project.manifest = validation.manifest;

        if ("name" in project.manifest) {
          project.name = project.manifest.name;
        } else {
          project.name = AppManager.DEFAULT_PROJECT_NAME;
        }
      } else {
        project.manifest = null;
        project.icon = AppManager.DEFAULT_PROJECT_ICON;
        project.name = AppManager.DEFAULT_PROJECT_NAME;
      }

      project.validationStatus = "valid";

      if (validation.warnings.length > 0) {
        project.warningsCount = validation.warnings.length;
        project.warnings = validation.warnings;
        project.validationStatus = "warning";
      } else {
        project.warnings = "";
        project.warningsCount = 0;
      }

      if (validation.errors.length > 0) {
        project.errorsCount = validation.errors.length;
        project.errors = validation.errors;
        project.validationStatus = "error";
      } else {
        project.errors = "";
        project.errorsCount = 0;
      }

      if (project.warningsCount && project.errorsCount) {
        project.validationStatus = "error warning";
      }

      if (project.type === "hosted" && project.location !== validation.manifestURL) {
        yield AppProjects.updateLocation(project, validation.manifestURL);
      } else if (AppProjects.get(project.location)) {
        yield AppProjects.update(project);
      }

      if (AppManager.selectedProject === project) {
        AppManager.update("project-validated");
      }
    });
  },

  /* RUNTIME LIST */

  trackUSBRuntimes: function() {
    this._updateUSBRuntimes = this._updateUSBRuntimes.bind(this);
    Devices.on("register", this._updateUSBRuntimes);
    Devices.on("unregister", this._updateUSBRuntimes);
    Devices.on("addon-status-updated", this._updateUSBRuntimes);
    this._updateUSBRuntimes();
  },
  untrackUSBRuntimes: function() {
    Devices.off("register", this._updateUSBRuntimes);
    Devices.off("unregister", this._updateUSBRuntimes);
    Devices.off("addon-status-updated", this._updateUSBRuntimes);
  },
  _updateUSBRuntimes: function() {
    this.runtimeList.usb = [];
    for (let id of Devices.available()) {
      let r = new USBRuntime(id);
      this.runtimeList.usb.push(r);
      r.updateNameFromADB().then(
        () => {
          this.update("runtimelist");
          // Also update the runtime button label, if the currently selected
          // runtime name changes
          if (r == this.selectedRuntime) {
            this.update("runtime");
          }
        },
        () => {});
    }
    this.update("runtimelist");
  },

  get isWiFiScanningEnabled() {
    return Services.prefs.getBoolPref(WIFI_SCANNING_PREF);
  },
  scanForWiFiRuntimes: function() {
    if (!this.isWiFiScanningEnabled) {
      return;
    }
    discovery.scan();
  },
  trackWiFiRuntimes: function() {
    if (!this.isWiFiScanningEnabled) {
      return;
    }
    this._updateWiFiRuntimes = this._updateWiFiRuntimes.bind(this);
    discovery.on("devtools-device-added", this._updateWiFiRuntimes);
    discovery.on("devtools-device-updated", this._updateWiFiRuntimes);
    discovery.on("devtools-device-removed", this._updateWiFiRuntimes);
    this._updateWiFiRuntimes();
  },
  untrackWiFiRuntimes: function() {
    if (!this.isWiFiScanningEnabled) {
      return;
    }
    discovery.off("devtools-device-added", this._updateWiFiRuntimes);
    discovery.off("devtools-device-updated", this._updateWiFiRuntimes);
    discovery.off("devtools-device-removed", this._updateWiFiRuntimes);
  },
  _updateWiFiRuntimes: function() {
    this.runtimeList.wifi = [];
    for (let device of discovery.getRemoteDevicesWithService("devtools")) {
      this.runtimeList.wifi.push(new WiFiRuntime(device));
    }
    this.update("runtimelist");
  },

  trackSimulatorRuntimes: function() {
    this._updateSimulatorRuntimes = this._updateSimulatorRuntimes.bind(this);
    Simulator.on("register", this._updateSimulatorRuntimes);
    Simulator.on("unregister", this._updateSimulatorRuntimes);
    this._updateSimulatorRuntimes();
  },
  untrackSimulatorRuntimes: function() {
    Simulator.off("register", this._updateSimulatorRuntimes);
    Simulator.off("unregister", this._updateSimulatorRuntimes);
  },
  _updateSimulatorRuntimes: function() {
    this.runtimeList.simulator = [];
    for (let version of Simulator.availableVersions()) {
      this.runtimeList.simulator.push(new SimulatorRuntime(version));
    }
    this.update("runtimelist");
  },

  writeManifest: function(project) {
    if (project.type != "packaged") {
      return promise.reject("Not a packaged app");
    }

    if (!project.manifest) {
      project.manifest = {};
    }

    let folder = project.location;
    let manifestPath = OS.Path.join(folder, "manifest.webapp");
    let text = JSON.stringify(project.manifest, null, 2);
    let encoder = new TextEncoder();
    let array = encoder.encode(text);
    return OS.File.writeAtomic(manifestPath, array, {tmpPath: manifestPath + ".tmp"});
  },
}

EventEmitter.decorate(AppManager);
