/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

const promise = require("promise");
const {TargetFactory} = require("devtools/client/framework/target");
const Services = require("Services");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const EventEmitter = require("devtools/shared/event-emitter");
const {TextEncoder, OS} = Cu.import("resource://gre/modules/osfile.jsm", {});
const {AppProjects} = require("devtools/client/webide/modules/app-projects");
const TabStore = require("devtools/client/webide/modules/tab-store");
const {AppValidator} = require("devtools/client/webide/modules/app-validator");
const {ConnectionManager, Connection} = require("devtools/shared/client/connection-manager");
const {AppActorFront} = require("devtools/shared/apps/app-actor-front");
const {getDeviceFront} = require("devtools/shared/fronts/device");
const {getPreferenceFront} = require("devtools/shared/fronts/preference");
const {getSettingsFront} = require("devtools/shared/fronts/settings");
const {Task} = require("devtools/shared/task");
const {RuntimeScanners, RuntimeTypes} = require("devtools/client/webide/modules/runtimes");
const {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const Telemetry = require("devtools/client/shared/telemetry");
const {ProjectBuilding} = require("./build");

const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

var AppManager = exports.AppManager = {

  DEFAULT_PROJECT_ICON: "chrome://webide/skin/default-app-icon.png",
  DEFAULT_PROJECT_NAME: "--",

  _initialized: false,

  init: function () {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    let port = Services.prefs.getIntPref("devtools.debugger.remote-port");
    this.connection = ConnectionManager.createConnection("localhost", port);
    this.onConnectionChanged = this.onConnectionChanged.bind(this);
    this.connection.on(Connection.Events.STATUS_CHANGED, this.onConnectionChanged);

    this.tabStore = new TabStore(this.connection);
    this.onTabList = this.onTabList.bind(this);
    this.onTabNavigate = this.onTabNavigate.bind(this);
    this.onTabClosed = this.onTabClosed.bind(this);
    this.tabStore.on("tab-list", this.onTabList);
    this.tabStore.on("navigate", this.onTabNavigate);
    this.tabStore.on("closed", this.onTabClosed);

    this._clearRuntimeList();
    this._rebuildRuntimeList = this._rebuildRuntimeList.bind(this);
    RuntimeScanners.on("runtime-list-updated", this._rebuildRuntimeList);
    RuntimeScanners.enable();
    this._rebuildRuntimeList();

    this.onInstallProgress = this.onInstallProgress.bind(this);

    this._telemetry = new Telemetry();
  },

  destroy: function () {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this.selectedProject = null;
    this.selectedRuntime = null;
    RuntimeScanners.off("runtime-list-updated", this._rebuildRuntimeList);
    RuntimeScanners.disable();
    this.runtimeList = null;
    this.tabStore.off("tab-list", this.onTabList);
    this.tabStore.off("navigate", this.onTabNavigate);
    this.tabStore.off("closed", this.onTabClosed);
    this.tabStore.destroy();
    this.tabStore = null;
    this.connection.off(Connection.Events.STATUS_CHANGED, this.onConnectionChanged);
    this._listTabsResponse = null;
    this.connection.disconnect();
    this.connection = null;
  },

  /**
   * This module emits various events when state changes occur.  The basic event
   * naming scheme is that event "X" means "X has changed" or "X is available".
   * Some names are more detailed to clarify their precise meaning.
   *
   * The events this module may emit include:
   *   before-project:
   *     The selected project is about to change.  The event includes a special
   *     |cancel| callback that will abort the project change if desired.
   *   connection:
   *     The connection status has changed (connected, disconnected, etc.)
   *   install-progress:
   *     A project being installed to a runtime has made further progress.  This
   *     event contains additional details about exactly how far the process is
   *     when such information is available.
   *   project:
   *     The selected project has changed.
   *   project-started:
   *     The selected project started running on the connected runtime.
   *   project-stopped:
   *     The selected project stopped running on the connected runtime.
   *   project-removed:
   *     The selected project was removed from the project list.
   *   project-validated:
   *     The selected project just completed validation.  As part of validation,
   *     many pieces of metadata about the project are refreshed, including its
   *     name, manifest details, etc.
   *   runtime:
   *     The selected runtime has changed.
   *   runtime-apps-icons:
   *     The list of URLs for the runtime app icons are available.
   *   runtime-global-actors:
   *     The list of global actors for the entire runtime (but not actors for a
   *     specific tab or app) are now available, so we can test for features
   *     like preferences and settings.
   *   runtime-details:
   *     The selected runtime's details have changed, such as its user-visible
   *     name.
   *   runtime-list:
   *     The list of available runtimes has changed, or any of the user-visible
   *     details (like names) for the non-selected runtimes has changed.
   *   runtime-telemetry:
   *     Detailed runtime telemetry has been recorded.  Used by tests.
   *   runtime-targets:
   *     The list of remote runtime targets available from the currently
   *     connected runtime (such as tabs or apps) has changed, or any of the
   *     user-visible details (like names) for the non-selected runtime targets
   *     has changed.  This event includes |type| in the details, to distinguish
   *     "apps" and "tabs".
   */
  update: function (what, details) {
    // Anything we want to forward to the UI
    this.emit("app-manager-update", what, details);
  },

  reportError: function (l10nProperty, ...l10nArgs) {
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

  onConnectionChanged: function () {
    console.log("Connection status changed: " + this.connection.status);

    if (this.connection.status == Connection.Status.DISCONNECTED) {
      this.selectedRuntime = null;
    }

    if (!this.connected) {
      if (this._appsFront) {
        this._appsFront.off("install-progress", this.onInstallProgress);
        this._appsFront.unwatchApps();
        this._appsFront = null;
      }
      this._listTabsResponse = null;
    } else {
      this.connection.client.listTabs((response) => {
        if (response.webappsActor) {
          let front = new AppActorFront(this.connection.client,
                                        response);
          front.on("install-progress", this.onInstallProgress);
          front.watchApps(() => this.checkIfProjectIsRunning())
          .then(() => {
            // This can't be done earlier as many operations
            // in the apps actor require watchApps to be called
            // first.
            this._appsFront = front;
            this._listTabsResponse = response;
            this._recordRuntimeInfo();
            this.update("runtime-global-actors");
          })
          .then(() => {
            this.checkIfProjectIsRunning();
            this.update("runtime-targets", { type: "apps" });
            front.fetchIcons().then(() => this.update("runtime-apps-icons"));
          });
        } else {
          this._listTabsResponse = response;
          this._recordRuntimeInfo();
          this.update("runtime-global-actors");
        }
      });
    }

    this.update("connection");
  },

  get connected() {
    return this.connection &&
           this.connection.status == Connection.Status.CONNECTED;
  },

  get apps() {
    if (this._appsFront) {
      return this._appsFront.apps;
    } else {
      return new Map();
    }
  },

  onInstallProgress: function (event, details) {
    this.update("install-progress", details);
  },

  isProjectRunning: function () {
    if (this.selectedProject.type == "mainProcess" ||
        this.selectedProject.type == "tab") {
      return true;
    }

    let app = this._getProjectFront(this.selectedProject);
    return app && app.running;
  },

  checkIfProjectIsRunning: function () {
    if (this.selectedProject) {
      if (this.isProjectRunning()) {
        this.update("project-started");
      } else {
        this.update("project-stopped");
      }
    }
  },

  listTabs: function () {
    return this.tabStore.listTabs();
  },

  onTabList: function () {
    this.update("runtime-targets", { type: "tabs" });
  },

  // TODO: Merge this into TabProject as part of project-agnostic work
  onTabNavigate: function () {
    this.update("runtime-targets", { type: "tabs" });
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

  onTabClosed: function () {
    if (this.selectedProject.type !== "tab") {
      return;
    }
    this.selectedProject = null;
  },

  reloadTab: function () {
    if (this.selectedProject && this.selectedProject.type != "tab") {
      return promise.reject("tried to reload non-tab project");
    }
    return this.getTarget().then(target => {
      target.activeTab.reload();
    }, console.error.bind(console));
  },

  getTarget: function () {
    if (this.selectedProject.type == "mainProcess") {
      // Fx >=39 exposes a ChromeActor to debug the main process
      if (this.connection.client.mainRoot.traits.allowChromeProcess) {
        return this.connection.client.getProcess()
                   .then(aResponse => {
                     return TargetFactory.forRemoteTab({
                       form: aResponse.form,
                       client: this.connection.client,
                       chrome: true
                     });
                   });
      } else {
        // Fx <39 exposes tab actors on the root actor
        return TargetFactory.forRemoteTab({
          form: this._listTabsResponse,
          client: this.connection.client,
          chrome: true,
          isTabActor: false
        });
      }
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
        } catch (e) {}
        let deferred = promise.defer();
        setTimeout(deferred.resolve, 500);
        yield deferred.promise;
      }

      AppManager.reportError("error_cantConnectToApp", app.manifest.manifestURL);
      throw new Error("can't connect to app");
    });
  },

  getProjectManifestURL: function (project) {
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

  _getProjectFront: function (project) {
    let manifest = this.getProjectManifestURL(project);
    if (manifest && this._appsFront) {
      return this._appsFront.apps.get(manifest);
    }
    return null;
  },

  _selectedProject: null,
  set selectedProject(project) {
    // A regular comparison doesn't work as we recreate a new object every time
    let prev = this._selectedProject;
    if (!prev && !project) {
      return;
    } else if (prev && project && prev.type === project.type) {
      let type = project.type;
      if (type === "runtimeApp") {
        if (prev.app.manifestURL === project.app.manifestURL) {
          return;
        }
      } else if (type === "tab") {
        if (prev.app.actor === project.app.actor) {
          return;
        }
      } else if (type === "packaged" || type === "hosted") {
        if (prev.location === project.location) {
          return;
        }
      } else if (type === "mainProcess") {
        return;
      } else {
        throw new Error("Unsupported project type: " + type);
      }
    }

    let cancelled = false;
    this.update("before-project", { cancel: () => { cancelled = true; } });
    if (cancelled) {
      return;
    }

    this._selectedProject = project;

    // Clear out tab store's selected state, if any
    this.tabStore.selectedTab = null;

    if (project) {
      if (project.type == "packaged" ||
          project.type == "hosted") {
        this.validateAndUpdateProject(project);
      }
      if (project.type == "tab") {
        this.tabStore.selectedTab = project.app;
      }
    }

    this.update("project");
    this.checkIfProjectIsRunning();
  },
  get selectedProject() {
    return this._selectedProject;
  },

  removeSelectedProject: Task.async(function* () {
    let location = this.selectedProject.location;
    AppManager.selectedProject = null;
    // If the user cancels the removeProject operation, don't remove the project
    if (AppManager.selectedProject != null) {
      return;
    }

    yield AppProjects.remove(location);
    AppManager.update("project-removed");
  }),

  packageProject: Task.async(function* (project) {
    if (!project) {
      return;
    }
    if (project.type == "packaged" ||
        project.type == "hosted") {
      yield ProjectBuilding.build({
        project: project,
        logger: this.update.bind(this, "pre-package")
      });
    }
  }),

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

  connectToRuntime: function (runtime) {

    if (this.connected && this.selectedRuntime === runtime) {
      // Already connected
      return promise.resolve();
    }

    let deferred = promise.defer();

    this.disconnectRuntime().then(() => {
      this.selectedRuntime = runtime;

      let onConnectedOrDisconnected = () => {
        this.connection.off(Connection.Events.CONNECTED, onConnectedOrDisconnected);
        this.connection.off(Connection.Events.DISCONNECTED, onConnectedOrDisconnected);
        if (this.connected) {
          deferred.resolve();
        } else {
          deferred.reject();
        }
      };
      this.connection.on(Connection.Events.CONNECTED, onConnectedOrDisconnected);
      this.connection.on(Connection.Events.DISCONNECTED, onConnectedOrDisconnected);
      try {
        // Reset the connection's state to defaults
        this.connection.resetOptions();
        // Only watch for errors here.  Final resolution occurs above, once
        // we've reached the CONNECTED state.
        this.selectedRuntime.connect(this.connection)
                            .then(null, e => deferred.reject(e));
      } catch (e) {
        deferred.reject(e);
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
    }).catch(() => {
      // Empty rejection handler to silence uncaught rejection warnings
      // |connectToRuntime| caller should listen for rejections.
      // Bug 1121100 may find a better way to silence these.
    });

    return deferred.promise;
  },

  _recordRuntimeInfo: Task.async(function* () {
    if (!this.connected) {
      return;
    }
    let runtime = this.selectedRuntime;
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_TYPE",
                             runtime.type || "UNKNOWN", true);
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_ID",
                             runtime.id || "unknown", true);
    if (!this.deviceFront) {
      this.update("runtime-telemetry");
      return;
    }
    let d = yield this.deviceFront.getDescription();
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_PROCESSOR",
                             d.processor, true);
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_OS",
                             d.os, true);
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_PLATFORM_VERSION",
                             d.platformversion, true);
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_APP_TYPE",
                             d.apptype, true);
    this._telemetry.logKeyed("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_VERSION",
                             d.version, true);
    this.update("runtime-telemetry");
  }),

  isMainProcessDebuggable: function () {
    // Fx <39 exposes chrome tab actors on RootActor
    // Fx >=39 exposes a dedicated actor via getProcess request
    return this.connection.client &&
           this.connection.client.mainRoot &&
           this.connection.client.mainRoot.traits.allowChromeProcess ||
           (this._listTabsResponse &&
            this._listTabsResponse.consoleActor);
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

  get settingsFront() {
    if (!this._listTabsResponse) {
      return null;
    }
    return getSettingsFront(this.connection.client, this._listTabsResponse);
  },

  disconnectRuntime: function () {
    if (!this.connected) {
      return promise.resolve();
    }
    let deferred = promise.defer();
    this.connection.once(Connection.Events.DISCONNECTED, () => deferred.resolve());
    this.connection.disconnect();
    return deferred.promise;
  },

  launchRuntimeApp: function () {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return promise.reject("attempting to launch a non-runtime app");
    }
    let app = this._getProjectFront(this.selectedProject);
    return app.launch();
  },

  launchOrReloadRuntimeApp: function () {
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

  runtimeCanHandleApps: function () {
    return !!this._appsFront;
  },

  installAndRunProject: function () {
    let project = this.selectedProject;

    if (!project || (project.type != "packaged" && project.type != "hosted")) {
      console.error("Can't install project. Unknown type of project.");
      return promise.reject("Can't install");
    }

    if (!this._listTabsResponse) {
      this.reportError("error_cantInstallNotFullyConnected");
      return promise.reject("Can't install");
    }

    if (!this._appsFront) {
      console.error("Runtime doesn't have a webappsActor");
      return promise.reject("Can't install");
    }

    return Task.spawn(function* () {
      let self = AppManager;

      // Package and validate project
      yield self.packageProject(project);
      yield self.validateAndUpdateProject(project);

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
        let packageDir = yield ProjectBuilding.getPackageDir(project);
        console.log("Installing app from " + packageDir);

        response = yield self._appsFront.installPackaged(packageDir,
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

      // Addons don't have any document to load (yet?)
      // So that there is no need to run them, installing is enough
      if (project.manifest.manifest_version || project.manifest.role === "addon") {
        return;
      }

      let {app} = response;
      if (!app.running) {
        let deferred = promise.defer();
        self.on("app-manager-update", function onUpdate(event, what) {
          if (what == "project-started") {
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

  stopRunningApp: function () {
    let app = this._getProjectFront(this.selectedProject);
    return app.close();
  },

  /* PROJECT VALIDATION */

  validateAndUpdateProject: function (project) {
    if (!project) {
      return promise.reject();
    }

    return Task.spawn(function* () {

      let packageDir = yield ProjectBuilding.getPackageDir(project);
      let validation = new AppValidator({
        type: project.type,
        // Build process may place the manifest in a non-root directory
        location: packageDir
      });

      yield validation.validate();

      if (validation.manifest) {
        let manifest = validation.manifest;
        let iconPath;
        if (manifest.icons) {
          let size = Object.keys(manifest.icons).sort((a, b) => b - a)[0];
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
            let projectFolder = FileUtils.File(packageDir);
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

  _clearRuntimeList: function () {
    this.runtimeList = {
      usb: [],
      wifi: [],
      simulator: [],
      other: []
    };
  },

  _rebuildRuntimeList: function () {
    let runtimes = RuntimeScanners.listRuntimes();
    this._clearRuntimeList();

    // Reorganize runtimes by type
    for (let runtime of runtimes) {
      switch (runtime.type) {
        case RuntimeTypes.USB:
          this.runtimeList.usb.push(runtime);
          break;
        case RuntimeTypes.WIFI:
          this.runtimeList.wifi.push(runtime);
          break;
        case RuntimeTypes.SIMULATOR:
          this.runtimeList.simulator.push(runtime);
          break;
        default:
          this.runtimeList.other.push(runtime);
      }
    }

    this.update("runtime-details");
    this.update("runtime-list");
  },

  /* MANIFEST UTILS */

  writeManifest: function (project) {
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
};

EventEmitter.decorate(AppManager);
