/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {AppProjects} = require("devtools/client/app-manager/app-projects");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const utils = require("devtools/client/webide/modules/utils");
const Telemetry = require("devtools/client/shared/telemetry");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

var ProjectList;

module.exports = ProjectList = function(win, parentWindow) {
  EventEmitter.decorate(this);
  this._doc = win.document;
  this._UI = parentWindow.UI;
  this._parentWindow = parentWindow;
  this._telemetry = new Telemetry();
  this._panelNodeEl = "div";

  this.onWebIDEUpdate = this.onWebIDEUpdate.bind(this);
  this._UI.on("webide-update", this.onWebIDEUpdate);

  AppManager.init();
  this.appManagerUpdate = this.appManagerUpdate.bind(this);
  AppManager.on("app-manager-update", this.appManagerUpdate);
};

ProjectList.prototype = {
  get doc() {
    return this._doc;
  },

  appManagerUpdate: function(event, what, details) {
    // Got a message from app-manager.js
    // See AppManager.update() for descriptions of what these events mean.
    switch (what) {
      case "project-removed":
      case "runtime-apps-icons":
      case "runtime-targets":
      case "connection":
        this.update(details);
        break;
      case "project":
        this.updateCommands();
        this.update(details);
        break;
    };
  },

  onWebIDEUpdate: function(event, what, details) {
    if (what == "busy" || what == "unbusy") {
      this.updateCommands();
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
    let parentWindow = this._parentWindow;
    let self = this;
    return this._UI.busyUntil(Task.spawn(function*() {
      // Open newapp.xul, which will feed ret.location
      let ret = {location: null, testOptions: testOptions};
      parentWindow.openDialog("chrome://webide/content/newapp.xul", "newapp", "chrome,modal", ret);
      if (!ret.location)
        return;

      // Retrieve added project
      let project = AppProjects.get(ret.location);

      // Select project
      AppManager.selectedProject = project;

      self._telemetry.actionOccurred("webideNewProject");
    }), "creating new app");
  },

  importPackagedApp: function(location) {
    let parentWindow = this._parentWindow;
    let UI = this._UI;
    return UI.busyUntil(Task.spawn(function*() {
      let directory = utils.getPackagedDirectory(parentWindow, location);

      if (!directory) {
        // User cancelled directory selection
        return;
      }

      yield UI.importAndSelectApp(directory);
    }), "importing packaged app");
  },

  importHostedApp: function(location) {
    let parentWindow = this._parentWindow;
    let UI = this._UI;
    return UI.busyUntil(Task.spawn(function*() {
      let url = utils.getHostedURL(parentWindow, location);

      if (!url) {
        return;
      }

      yield UI.importAndSelectApp(url);
    }), "importing hosted app");
  },

  /**
   * opts: {
   *   panel: Object,     currenl project panel node
   *   name: String,      name of the project
   *   icon: String       path of the project icon
   * }
   */
  _renderProjectItem: function(opts) {
    let span = opts.panel.querySelector("span") || this._doc.createElement("span");
    span.textContent = opts.name;
    let icon = opts.panel.querySelector("img") || this._doc.createElement("img");
    icon.className = "project-image";
    icon.setAttribute("src", opts.icon);
    opts.panel.appendChild(icon);
    opts.panel.appendChild(span);
    opts.panel.setAttribute("title", opts.name);
  },

  refreshTabs: function() {
    if (AppManager.connected) {
      return AppManager.listTabs().then(() => {
        this.updateTabs();
      }).catch(console.error);
    }
  },

  updateTabs: function() {
    let tabsHeaderNode = this._doc.querySelector("#panel-header-tabs");
    let tabsNode = this._doc.querySelector("#project-panel-tabs");

    while (tabsNode.hasChildNodes()) {
      tabsNode.firstChild.remove();
    }

    if (!AppManager.connected) {
      tabsHeaderNode.setAttribute("hidden", "true");
      return;
    }

    let tabs = AppManager.tabStore.tabs;

    if (tabs.length > 0) {
      tabsHeaderNode.removeAttribute("hidden");
    } else {
      tabsHeaderNode.setAttribute("hidden", "true");
    }

    for (let i = 0; i < tabs.length; i++) {
      let tab = tabs[i];
      let URL = this._parentWindow.URL;
      let url;
      try {
        url = new URL(tab.url);
      } catch (e) {
        // Don't try to handle invalid URLs, especially from Valence.
        continue;
      }
      // Wanted to use nsIFaviconService here, but it only works for visited
      // tabs, so that's no help for any remote tabs.  Maybe some favicon wizard
      // knows how to get high-res favicons easily, or we could offer actor
      // support for this (bug 1061654).
      if (url.origin) {
        tab.favicon = url.origin + "/favicon.ico";
      }
      tab.name = tab.title || Strings.GetStringFromName("project_tab_loading");
      if (url.protocol.startsWith("http")) {
        tab.name = url.hostname + ": " + tab.name;
      }
      let panelItemNode = this._doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      tabsNode.appendChild(panelItemNode);
      this._renderProjectItem({
        panel: panelItemNode,
        name: tab.name,
        icon: tab.favicon || AppManager.DEFAULT_PROJECT_ICON
      });
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "tab",
          app: tab,
          icon: tab.favicon || AppManager.DEFAULT_PROJECT_ICON,
          location: tab.url,
          name: tab.name
        };
      }, true);
    }

    return promise.resolve();
  },

  updateApps: function() {
    let doc = this._doc;
    let runtimeappsHeaderNode = doc.querySelector("#panel-header-runtimeapps");
    let sortedApps = [];
    for (let [manifestURL, app] of AppManager.apps) {
      sortedApps.push(app);
    }
    sortedApps = sortedApps.sort((a, b) => {
      return a.manifest.name > b.manifest.name;
    });
    let mainProcess = AppManager.isMainProcessDebuggable();
    if (AppManager.connected && (sortedApps.length > 0 || mainProcess)) {
      runtimeappsHeaderNode.removeAttribute("hidden");
    } else {
      runtimeappsHeaderNode.setAttribute("hidden", "true");
    }

    let runtimeAppsNode = doc.querySelector("#project-panel-runtimeapps");
    while (runtimeAppsNode.hasChildNodes()) {
      runtimeAppsNode.firstChild.remove();
    }

    if (mainProcess) {
      let panelItemNode = doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      this._renderProjectItem({
        panel: panelItemNode,
        name: Strings.GetStringFromName("mainProcess_label"),
        icon: AppManager.DEFAULT_PROJECT_ICON
      });
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "mainProcess",
          name: Strings.GetStringFromName("mainProcess_label"),
          icon: AppManager.DEFAULT_PROJECT_ICON
        };
      }, true);
    }

    for (let i = 0; i < sortedApps.length; i++) {
      let app = sortedApps[i];
      let panelItemNode = doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      this._renderProjectItem({
        panel: panelItemNode,
        name: app.manifest.name,
        icon: app.iconURL || AppManager.DEFAULT_PROJECT_ICON
      });
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "runtimeApp",
          app: app.manifest,
          icon: app.iconURL || AppManager.DEFAULT_PROJECT_ICON,
          name: app.manifest.name
        };
      }, true);
    }

    return promise.resolve();
  },

  updateCommands: function() {
    let doc = this._doc;
    let newAppCmd;
    let packagedAppCmd;
    let hostedAppCmd;

    newAppCmd = doc.querySelector("#new-app");
    packagedAppCmd = doc.querySelector("#packaged-app");
    hostedAppCmd = doc.querySelector("#hosted-app");

    if (!newAppCmd || !packagedAppCmd || !hostedAppCmd) {
      return;
    }

    if (this._parentWindow.document.querySelector("window").classList.contains("busy")) {
      newAppCmd.setAttribute("disabled", "true");
      packagedAppCmd.setAttribute("disabled", "true");
      hostedAppCmd.setAttribute("disabled", "true");
      return;
    }

    newAppCmd.removeAttribute("disabled");
    packagedAppCmd.removeAttribute("disabled");
    hostedAppCmd.removeAttribute("disabled");
  },

  /**
   * Trigger an update of the project and remote runtime list.
   * @param options object (optional)
   *        An |options| object containing a type of |apps| or |tabs| will limit
   *        what is updated to only those sections.
   */
  update: function(options) {
    let deferred = promise.defer();

    if (options && options.type === "apps") {
      return this.updateApps();
    } else if (options && options.type === "tabs") {
      return this.updateTabs();
    }

    let doc = this._doc;
    let projectsNode = doc.querySelector("#project-panel-projects");

    while (projectsNode.hasChildNodes()) {
      projectsNode.firstChild.remove();
    }

    AppProjects.load().then(() => {
      let projects = AppProjects.store.object.projects;
      for (let i = 0; i < projects.length; i++) {
        let project = projects[i];
        let panelItemNode = doc.createElement(this._panelNodeEl);
        panelItemNode.className = "panel-item";
        projectsNode.appendChild(panelItemNode);
        if (!project.validationStatus) {
          // The result of the validation process (storing names, icons, …) is not stored in
          // the IndexedDB database when App Manager v1 is used.
          // We need to run the validation again and update the name and icon of the app.
          AppManager.validateAndUpdateProject(project).then(() => {
            this._renderProjectItem({
              panel: panelItemNode,
              name: project.name,
              icon: project.icon
            });
          });
        } else {
          this._renderProjectItem({
            panel: panelItemNode,
            name: project.name || AppManager.DEFAULT_PROJECT_NAME,
            icon: project.icon || AppManager.DEFAULT_PROJECT_ICON
          });
        }
        panelItemNode.addEventListener("click", () => {
          AppManager.selectedProject = project;
        }, true);
      }

      deferred.resolve();
    }, deferred.reject);

    // List remote apps and the main process, if they exist
    this.updateApps();

    // Build the tab list right now, so it's fast...
    this.updateTabs();

    // But re-list them and rebuild, in case any tabs navigated since the last
    // time they were listed.
    if (AppManager.connected) {
      AppManager.listTabs().then(() => {
        this.updateTabs();
      }).catch(console.error);
    }

    return deferred.promise;
  },

  destroy: function() {
    this._doc = null;
    AppManager.off("app-manager-update", this.appManagerUpdate);
    this._UI.off("webide-update", this.onWebIDEUpdate);
    this._UI = null;
    this._parentWindow = null;
    this._panelNodeEl = null;
  }
};
