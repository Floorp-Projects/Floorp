/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {TargetFactory} = require("devtools/client/framework/target");
const Services = require("Services");
const {FileUtils} = require("resource://gre/modules/FileUtils.jsm");
const EventEmitter = require("devtools/shared/event-emitter");
const {OS} = require("resource://gre/modules/osfile.jsm");
const {AppProjects} = require("devtools/client/webide/modules/app-projects");
const TabStore = require("devtools/client/webide/modules/tab-store");
const {AppValidator} = require("devtools/client/webide/modules/app-validator");
const {ConnectionManager, Connection} = require("devtools/shared/client/connection-manager");
const {getDeviceFront} = require("devtools/shared/fronts/device");
const {getPreferenceFront} = require("devtools/shared/fronts/preference");
const {RuntimeScanners, RuntimeTypes} = require("devtools/client/webide/modules/runtimes");
const {NetUtil} = require("resource://gre/modules/NetUtil.jsm");
const Telemetry = require("devtools/client/shared/telemetry");

const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

var AppManager = exports.AppManager = {

  DEFAULT_PROJECT_ICON: "chrome://webide/skin/default-app-icon.png",
  DEFAULT_PROJECT_NAME: "--",

  _initialized: false,

  init: function() {
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

    this._telemetry = new Telemetry();
  },

  destroy: function() {
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
    console.log("Connection status changed: " + this.connection.status);

    if (this.connection.status == Connection.Status.DISCONNECTED) {
      this.selectedRuntime = null;
    }

    if (!this.connected) {
      this._listTabsResponse = null;
    } else {
      this.connection.client.listTabs().then((response) => {
        this._listTabsResponse = response;
        this._recordRuntimeInfo();
        this.update("runtime-global-actors");
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
    }
    return new Map();
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
        this.update("project-started");
      } else {
        this.update("project-stopped");
      }
    }
  },

  listTabs: function() {
    return this.tabStore.listTabs();
  },

  onTabList: function() {
    this.update("runtime-targets", { type: "tabs" });
  },

  // TODO: Merge this into TabProject as part of project-agnostic work
  onTabNavigate: function() {
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

  onTabClosed: function() {
    if (this.selectedProject.type !== "tab") {
      return;
    }
    this.selectedProject = null;
  },

  reloadTab: function() {
    if (this.selectedProject && this.selectedProject.type != "tab") {
      return Promise.reject("tried to reload non-tab project");
    }
    return this.getTarget().then(target => {
      target.activeTab.reload();
    }, console.error);
  },

  getTarget: function() {
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
      }
        // Fx <39 exposes tab actors on the root actor
      return TargetFactory.forRemoteTab({
          form: this._listTabsResponse,
          client: this.connection.client,
          chrome: true,
          isTabActor: false
      });
    }

    if (this.selectedProject.type == "tab") {
      return this.tabStore.getTargetForTab();
    }

    let app = this._getProjectFront(this.selectedProject);
    if (!app) {
      return Promise.reject("Can't find app front for selected project");
    }

    return (async function() {
      // Once we asked the app to launch, the app isn't necessary completely loaded.
      // launch request only ask the app to launch and immediatly returns.
      // We have to keep trying to get app tab actors required to create its target.

      for (let i = 0; i < 10; i++) {
        try {
          return await app.getTarget();
        } catch (e) {}
        return new Promise(resolve => {
          setTimeout(resolve, 500);
        });
      }

      AppManager.reportError("error_cantConnectToApp", app.manifest.manifestURL);
      throw new Error("can't connect to app");
    })();
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
    this.update("before-project", { cancel: () => {
      cancelled = true;
    } });
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

  async removeSelectedProject() {
    let location = this.selectedProject.location;
    AppManager.selectedProject = null;
    // If the user cancels the removeProject operation, don't remove the project
    if (AppManager.selectedProject != null) {
      return;
    }

    await AppProjects.remove(location);
    AppManager.update("project-removed");
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
    if (this.connected && this.selectedRuntime === runtime) {
      // Already connected
      return Promise.resolve();
    }

    let deferred = new Promise((resolve, reject) => {
      this.disconnectRuntime().then(() => {
        this.selectedRuntime = runtime;

        let onConnectedOrDisconnected = () => {
          this.connection.off(Connection.Events.CONNECTED, onConnectedOrDisconnected);
          this.connection.off(Connection.Events.DISCONNECTED, onConnectedOrDisconnected);
          if (this.connected) {
            resolve();
          } else {
            reject();
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
                              .catch(e => reject(e));
        } catch (e) {
          reject(e);
        }
      }, reject);
    });

    // Record connection result in telemetry
    let logResult = result => {
      this._telemetry.getHistogramById("DEVTOOLS_WEBIDE_CONNECTION_RESULT")
                     .add(result);
      if (runtime.type) {
        this._telemetry.getHistogramById(
          `DEVTOOLS_WEBIDE_${runtime.type}_CONNECTION_RESULT`).add(result);
      }
    };
    deferred.then(() => logResult(true), () => logResult(false));

    // If successful, record connection time in telemetry
    deferred.then(() => {
      const timerId = "DEVTOOLS_WEBIDE_CONNECTION_TIME_SECONDS";
      this._telemetry.start(timerId, this);
      this.connection.once(Connection.Events.STATUS_CHANGED, () => {
        this._telemetry.finish(timerId, this);
      });
    }).catch(() => {
      // Empty rejection handler to silence uncaught rejection warnings
      // |connectToRuntime| caller should listen for rejections.
      // Bug 1121100 may find a better way to silence these.
    });

    return deferred;
  },

  async _recordRuntimeInfo() {
    if (!this.connected) {
      return;
    }
    let runtime = this.selectedRuntime;
    this._telemetry
        .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_TYPE")
        .add(runtime.type || "UNKNOWN", true);
    this._telemetry
        .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_ID")
        .add(runtime.id || "unknown", true);
    if (!this.deviceFront) {
      this.update("runtime-telemetry");
      return;
    }
    let d = await this.deviceFront.getDescription();
    this._telemetry
      .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_PROCESSOR")
      .add(d.processor, true);
    this._telemetry
      .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_OS")
      .add(d.os, true);
    this._telemetry
      .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_PLATFORM_VERSION")
      .add(d.platformversion, true);
    this._telemetry
        .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_APP_TYPE")
        .add(d.apptype, true);
    this._telemetry
        .getKeyedHistogramById("DEVTOOLS_WEBIDE_CONNECTED_RUNTIME_VERSION")
        .add(d.version, true);
    this.update("runtime-telemetry");
  },

  isMainProcessDebuggable: function() {
    // Fx <39 exposes chrome tab actors on RootActor
    // Fx >=39 exposes a dedicated actor via getProcess request
    return this.connection.client &&
           this.connection.client.mainRoot &&
           this.connection.client.mainRoot.traits.allowChromeProcess ||
           (this._listTabsResponse &&
            this._listTabsResponse.consoleActor);
  },

  get listTabsForm() {
    return this._listTabsResponse;
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
    if (!this.connected) {
      return Promise.resolve();
    }

    return new Promise(resolve => {
      this.connection.once(Connection.Events.DISCONNECTED, () => resolve());
      this.connection.disconnect();
    });
  },

  launchRuntimeApp: function() {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return Promise.reject("attempting to launch a non-runtime app");
    }
    let app = this._getProjectFront(this.selectedProject);
    return app.launch();
  },

  launchOrReloadRuntimeApp: function() {
    if (this.selectedProject && this.selectedProject.type != "runtimeApp") {
      return Promise.reject("attempting to launch / reload a non-runtime app");
    }
    let app = this._getProjectFront(this.selectedProject);
    if (!app.running) {
      return app.launch();
    }
    return app.reload();
  },

  runtimeCanHandleApps: function() {
    return !!this._appsFront;
  },

  installAndRunProject: function() {
    let project = this.selectedProject;

    if (!project || (project.type != "packaged" && project.type != "hosted")) {
      console.error("Can't install project. Unknown type of project.");
      return Promise.reject("Can't install");
    }

    if (!this._listTabsResponse) {
      this.reportError("error_cantInstallNotFullyConnected");
      return Promise.reject("Can't install");
    }

    if (!this._appsFront) {
      console.error("Runtime doesn't have a webappsActor");
      return Promise.reject("Can't install");
    }

    return (async function() {
      let self = AppManager;

      // Validate project
      await self.validateAndUpdateProject(project);

      if (project.errorsCount > 0) {
        self.reportError("error_cantInstallValidationErrors");
        return;
      }

      if (project.type != "packaged" && project.type != "hosted") {
        return Promise.reject("Don't know how to install project");
      }

      let response;
      if (project.type == "packaged") {
        let packageDir = project.location;
        console.log("Installing app from " + packageDir);

        response = await self._appsFront.installPackaged(packageDir,
                                                         project.packagedAppOrigin);

        // If the packaged app specified a custom origin override,
        // we need to update the local project origin
        project.packagedAppOrigin = response.appId;
        // And ensure the indexed db on disk is also updated
        AppProjects.update(project);
      }

      if (project.type == "hosted") {
        let manifestURLObject = Services.io.newURI(project.location);
        let origin = Services.io.newURI(manifestURLObject.prePath);
        let appId = origin.host;
        let metadata = {
          origin: origin.spec,
          manifestURL: project.location
        };
        response = await self._appsFront.installHosted(appId,
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
        let deferred = new Promise(resolve => {
          self.on("app-manager-update", function onUpdate(what) {
            if (what == "project-started") {
              self.off("app-manager-update", onUpdate);
              resolve();
            }
          });
        });
        await app.launch();
        await deferred;
      } else {
        await app.reload();
      }
    })();
  },

  stopRunningApp: function() {
    let app = this._getProjectFront(this.selectedProject);
    return app.close();
  },

  /* PROJECT VALIDATION */

  validateAndUpdateProject: function(project) {
    if (!project) {
      return Promise.reject();
    }

    return (async function() {
      let packageDir = project.location;
      let validation = new AppValidator({
        type: project.type,
        // Build process may place the manifest in a non-root directory
        location: packageDir
      });

      await validation.validate();

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
        } else if (project.type == "hosted") {
          let manifestURL = Services.io.newURI(project.location);
          let origin = Services.io.newURI(manifestURL.prePath);
          project.icon = Services.io.newURI(iconPath, null, origin).spec;
        } else if (project.type == "packaged") {
          let projectFolder = FileUtils.File(packageDir);
          let folderURI = Services.io.newFileURI(projectFolder).spec;
          project.icon = folderURI + iconPath.replace(/^\/|\\/, "");
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
        await AppProjects.updateLocation(project, validation.manifestURL);
      } else if (AppProjects.get(project.location)) {
        await AppProjects.update(project);
      }

      if (AppManager.selectedProject === project) {
        AppManager.update("project-validated");
      }
    })();
  },

  /* RUNTIME LIST */

  _clearRuntimeList: function() {
    this.runtimeList = {
      usb: [],
      wifi: [],
      other: []
    };
  },

  _rebuildRuntimeList: function() {
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
        default:
          this.runtimeList.other.push(runtime);
      }
    }

    this.update("runtime-details");
    this.update("runtime-list");
  },

  /* MANIFEST UTILS */

  writeManifest: function(project) {
    if (project.type != "packaged") {
      return Promise.reject("Not a packaged app");
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
