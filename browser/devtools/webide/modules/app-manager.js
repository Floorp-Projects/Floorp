/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {Simulator} = Cu.import("resource://gre/modules/devtools/Simulator.jsm");
const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js");
const {TextEncoder, OS}  = Cu.import("resource://gre/modules/osfile.jsm", {});
const {AppProjects} = require("devtools/app-manager/app-projects");
const WebappsStore = require("devtools/app-manager/webapps-store");
const {AppValidator} = require("devtools/app-manager/app-validator");
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const AppActorFront = require("devtools/app-actor-front");
const {getDeviceFront} = require("devtools/server/actors/device");

const Strings = Services.strings.createBundle("chrome://webide/content/webide.properties");

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
    this.webAppsStore = new WebappsStore(this.connection);

    this.runtimeList = {usb: [], simulator: []};
    this.trackUSBRuntimes();
    this.trackSimulatorRuntimes();
  },

  uninit: function() {
    this._unlistenToApps();
    this.selectedProject = null;
    this.selectedRuntime = null;
    this.untrackUSBRuntimes();
    this.untrackSimulatorRuntimes();
    this._runningApps.clear();
    this.runtimeList = null;
    this.connection.off(Connection.Events.STATUS_CHANGED, this.onConnectionChanged);
    this.webAppsStore.destroy();
    this.webAppsStore = null;
    this._listTabsResponse = null;
    this.connection.disconnect();
    this.connection = null;
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
      this._runningApps.clear();
      this._unlistenToApps();
      this._listTabsResponse = null;
    } else {
      this.connection.client.listTabs((response) => {
        this._listenToApps();
        this._listTabsResponse = response;
        this._getRunningApps();
      });
    }

    this.update("connection");
  },

  _runningApps: new Set(),
  _getRunningApps: function() {
    let client = this.connection.client;
    if (!this._listTabsResponse.webappsActor) {
      return;
    }
    let request = {
      to: this._listTabsResponse.webappsActor,
      type: "listRunningApps"
    };
    client.request(request, (res) => {
      if (res.error) {
        this.reportError("error_listRunningApps");
        console.error("listRunningApps error: " + res.error);
      }
      for (let m of res.apps) {
        this._runningApps.add(m);
      }
    });
    this.checkIfProjectIsRunning();
  },
  _listenToApps: function() {
    let client = this.connection.client;
    client.addListener("appOpen", (type, { manifestURL }) => {
      this._runningApps.add(manifestURL);
      this.checkIfProjectIsRunning();
    });

    client.addListener("appClose", (type, { manifestURL }) => {
      this._runningApps.delete(manifestURL);
      this.checkIfProjectIsRunning();
    });

    client.addListener("appUninstall", (type, { manifestURL }) => {
      this._runningApps.delete(manifestURL);
      this.checkIfProjectIsRunning();
    });
  },
  _unlistenToApps: function() {
    // Is that even possible?
    // connection.client is null now.
  },

  isProjectRunning: function() {
    let manifest = this.getProjectManifestURL(this.selectedProject);
    return manifest && this._runningApps.has(manifest);
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

  getTarget: function() {
    let manifest = this.getProjectManifestURL(this.selectedProject);
    let name = this.selectedProject.name;
    if (manifest) {
      let client = this.connection.client;
      let actor = this._listTabsResponse.webappsActor;

      let promise = AppActorFront.getTargetForApp(client, actor, manifest);
      promise.then(( ) => { },
                   (e) => {
                     this.reportError("error_cantConnectToApp", manifestURL);
                     console.error("Can't connect to app: " + e)
                   });
      return promise;

    }
    console.error("Can't find manifestURL for selected project");
    return promise.reject();
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

  _selectedProject: null,
  set selectedProject(value) {
    if (value != this.selectedProject) {
      this._selectedProject = value;

      if (this.selectedProject) {
        if (this.selectedProject.type == "runtimeApp") {
          this.runRuntimeApp();
        } else {
          this.validateProject(this.selectedProject);
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
    if (!value &&
      this.selectedProject &&
      this.selectedProject.type == "runtimeApp") {
      this.selectedProject = null;
    }
    this.update("runtime");
  },

  get selectedRuntime() {
    return this._selectedRuntime;
  },

  connectToRuntime: function(runtime) {
    if (this.connection.status == Connection.Status.CONNECTED) {
      return promise.reject("Already connected");
    }
    this.selectedRuntime = runtime;
    let deferred = promise.defer();

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
    this.selectedRuntime.connect(this.connection).then(
      () => {},
      () => {deferred.reject()});

    return deferred.promise;
  },

  get deviceFront() {
    if (!this._listTabsResponse) {
      return null;
    }
    return getDeviceFront(this.connection.client, this._listTabsResponse);
  },

  disconnectRuntime: function() {
    if (this.connection.status != Connection.Status.CONNECTED) {
      return promise.reject("Already disconnected");
    }
    let deferred = promise.defer();
    this.connection.once(Connection.Events.DISCONNECTED, () => deferred.resolve());
    this.connection.disconnect();
    return deferred.promise;
  },

  runRuntimeApp: function() {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return promise.reject("attempting to run a non-runtime app");
    }
    let client = this.connection.client;
    let actor = this._listTabsResponse.webappsActor;
    let manifest = this.getProjectManifestURL(this.selectedProject);
    return AppActorFront.launchApp(client, actor, manifest);
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

    return this.validateProject(project).then(() => {

      if (project.errorsCount > 0) {
        this.reportError("error_cantInstallValidationErrors");
        return;
      }

      let client = this.connection.client;
      let actor = this._listTabsResponse.webappsActor;
      let installPromise;

      if (project.type == "packaged") {
        installPromise = AppActorFront.installPackaged(client, actor, project.location, project.packagedAppOrigin)
              .then(({ appId }) => {
                // If the packaged app specified a custom origin override,
                // we need to update the local project origin
                project.packagedAppOrigin = appId;
                // And ensure the indexed db on disk is also updated
                AppProjects.update(project);
              });
      }

      if (project.type == "hosted") {
        let manifestURLObject = Services.io.newURI(project.location, null, null);
        let origin = Services.io.newURI(manifestURLObject.prePath, null, null);
        let appId = origin.host;
        let metadata = {
          origin: origin.spec,
          manifestURL: project.location
        };
        installPromise = AppActorFront.installHosted(client, actor, appId, metadata, project.manifest);
      }

      if (!installPromise) {
        return promise.reject("Can't install");
      }

      return installPromise.then(() => {
        let manifest = this.getProjectManifestURL(project);
        if (!this._runningApps.has(manifest)) {
          console.log("Launching app: " + project.name);
          AppActorFront.launchApp(client, actor, manifest);
        } else {
          console.log("Reloading app: " + project.name);
          AppActorFront.reloadApp(client, actor, manifest);
        }
      });

    }, console.error);
  },

  stopRunningApp: function() {
    let client = this.connection.client;
    let actor = this._listTabsResponse.webappsActor;
    let manifest = this.getProjectManifestURL(this.selectedProject);
    return AppActorFront.closeApp(client, actor, manifest);
  },

  /* PROJECT VALIDATION */

  validateProject: function(project) {
    if (!project) {
      return promise.reject();
    }

    let validation = new AppValidator(project);
    return validation.validate()
      .then(() => {
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

        if (this.selectedProject === project) {
          this.update("project-validated");
        }

        if (AppProjects.get(project.location)) {
          AppProjects.update(project);
        }

        return project;
      }, console.error);
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
      this.runtimeList.usb.push(new USBRuntime(id));
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

/* RUNTIMES */

function USBRuntime(id) {
  this.id = id;
}

USBRuntime.prototype = {
  connect: function(connection) {
    let device = Devices.getByName(this.id);
    if (!device) {
      console.error("Can't find device: " + id);
      return promise.reject();
    }
    return device.connect().then((port) => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  },
  getID: function() {
    return this.id;
  },
  getName: function() {
    return this.id;
  },
}

function SimulatorRuntime(version) {
  this.version = version;
}

SimulatorRuntime.prototype = {
  connect: function(connection) {
    let port = ConnectionManager.getFreeTCPPort();
    let simulator = Simulator.getByVersion(this.version);
    if (!simulator || !simulator.launch) {
      console.error("Can't find simulator: " + this.version);
      return promise.reject();
    }
    return simulator.launch({port: port}).then(() => {
      connection.port = port;
      connection.keepConnecting = true;
      connection.connect();
    });
  },
  getID: function() {
    return this.version;
  },
  getName: function() {
    return this.version;
  },
}
