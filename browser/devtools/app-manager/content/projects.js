/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
Cu.import("resource:///modules/devtools/gDevTools.jsm");
const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const {AppProjects} = require("devtools/app-manager/app-projects");
const {AppValidator} = require("devtools/app-manager/app-validator");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {installHosted, installPackaged} = require("devtools/app-actor-front");

const promise = require("sdk/core/promise");

window.addEventListener("message", function(event) {
  try {
    let json = JSON.parse(event.data);
    if (json.name == "connection") {
      let cid = parseInt(json.cid);
      for (let c of ConnectionManager.connections) {
        if (c.uid == cid) {
          UI.connection = c;
          UI.onNewConnection();
          break;
        }
      }
    }
  } catch(e) {}
}, false);

let UI = {
  onload: function() {
    this.template = new Template(document.body, AppProjects.store, Utils.l10n);
    this.template.start();

    AppProjects.store.on("set", (event,path,value) => {
      if (path == "projects") {
        AppProjects.store.object.projects.forEach(UI.validate);
      }
    });
  },

  onNewConnection: function() {
    this.connection.on(Connection.Events.STATUS_CHANGED, () => this._onConnectionStatusChange());
    this._onConnectionStatusChange();
  },

  _onConnectionStatusChange: function() {
    if (this.connection.status != Connection.Status.CONNECTED) {
      document.body.classList.remove("connected");
      this.listTabsResponse = null;
    } else {
      document.body.classList.add("connected");
      this.connection.client.listTabs(
        response => {this.listTabsResponse = response}
      );
    }
  },

  _selectFolder: function() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, Utils.l10n("project.filePickerTitle"), Ci.nsIFilePicker.modeGetFolder);
    let res = fp.show();
    if (res != Ci.nsIFilePicker.returnCancel)
      return fp.file;
    return null;
  },

  addPackaged: function() {
    let folder = this._selectFolder();
    if (!folder)
      return;
    AppProjects.addPackaged(folder)
               .then(function (project) {
                 UI.validate(project);
                 UI.selectProject(project.location);
               });
  },

  addHosted: function() {
    let urlInput = document.querySelector("#url-input");
    let manifestURL = urlInput.value;
    AppProjects.addHosted(manifestURL)
               .then(function (project) {
                 UI.validate(project);
                 UI.selectProject(project.location);
               });
  },

  _getLocalIconURL: function(project, manifest) {
    let icon;
    if (manifest.icons) {
      let size = Object.keys(manifest.icons).sort(function(a, b) b - a)[0];
      if (size) {
        icon = manifest.icons[size];
      }
    }
    if (!icon)
      return null;
    if (project.type == "hosted") {
      let manifestURL = Services.io.newURI(project.location, null, null);
      let origin = Services.io.newURI(manifestURL.prePath, null, null);
      return Services.io.newURI(icon, null, origin).spec;
    } else if (project.type == "packaged") {
      let projectFolder = FileUtils.File(project.location);
      let folderURI = Services.io.newFileURI(projectFolder).spec;
      return folderURI + icon.replace(/^\/|\\/, "");
    }
  },

  validate: function(project) {
    let validation = new AppValidator(project);
    validation.validate()
              .then(function () {
                if (validation.manifest) {
                  project.name = validation.manifest.name;
                  project.icon = UI._getLocalIconURL(project, validation.manifest);
                  project.manifest = validation.manifest;
                }

                project.validationStatus = "valid";

                if (validation.warnings.length > 0) {
                  project.warningsCount = validation.warnings.length;
                  project.warnings = validation.warnings.join(",\n ");
                  project.validationStatus = "warning";
                } else {
                  project.warnings = "";
                  project.warningsCount = 0;
                }

                if (validation.errors.length > 0) {
                  project.errorsCount = validation.errors.length;
                  project.errors = validation.errors.join(",\n ");
                  project.validationStatus = "error";
                } else {
                  project.errors = "";
                  project.errorsCount = 0;
                }

              });

  },

  update: function(location) {
    let project = AppProjects.get(location);
    this.validate(project);
  },

  remove: function(location) {
    AppProjects.remove(location);
  },

  _getProjectManifestURL: function (project) {
    if (project.type == "packaged") {
      return "app://" + project.packagedAppOrigin + "/manifest.webapp";
    } else if (project.type == "hosted") {
      return project.location;
    }
  },

  install: function(button, location) {
    button.dataset.originalTextContent = button.textContent;
    button.textContent = Utils.l10n("project.installing");
    button.disabled = true;
    let project = AppProjects.get(location);
    let install;
    if (project.type == "packaged") {
      install = installPackaged(this.connection.client, this.listTabsResponse.webappsActor, project.location, project.packagedAppOrigin);
    } else {
      let manifestURLObject = Services.io.newURI(project.location, null, null);
      let origin = Services.io.newURI(manifestURLObject.prePath, null, null);
      let appId = origin.host;
      let metadata = {
        origin: origin.spec,
        manifestURL: project.location
      };
      install = installHosted(this.connection.client, this.listTabsResponse.webappsActor, appId, metadata, project.manifest);
    }
    install.then(function () {
      button.disabled = false;
      button.textContent = Utils.l10n("project.installed");
      setTimeout(function() {
        button.textContent = button.dataset.originalTextContent;
      }, 1500);
    },
    function (res) {
      button.disabled = false;
      let message = res.error + ": " + res.message;
      alert(message);
      this.connection.log(message);
    });
  },

  start: function(location) {
    let project = AppProjects.get(location);
    let request = {
      to: this.listTabsResponse.webappsActor,
      type: "launch",
      manifestURL: this._getProjectManifestURL(project)
    };
    this.connection.client.request(request, (res) => {

    });
  },

  stop: function(location) {
    let project = AppProjects.get(location);
    let request = {
      to: this.listTabsResponse.webappsActor,
      type: "close",
      manifestURL: this._getProjectManifestURL(project)
    };
    this.connection.client.request(request, (res) => {

    });
  },

  _getTargetForApp: function(manifest) { // FIXME <- will be implemented in bug 912476
    if (!this.listTabsResponse)
      return null;
    let actor = this.listTabsResponse.webappsActor;
    let deferred = promise.defer();
    let request = {
      to: actor,
      type: "getAppActor",
      manifestURL: manifest,
    }
    this.connection.client.request(request, (res) => {
      if (res.error) {
        deferred.reject(res.error);
      } else {
        let options = {
          form: res.actor,
          client: this.connection.client,
          chrome: false
        };

        devtools.TargetFactory.forRemoteTab(options).then((target) => {
          deferred.resolve(target)
        }, (error) => {
          deferred.reject(error);
        });
      }
    });
    return deferred.promise;
  },

  openToolbox: function(location) {
    let project = AppProjects.get(location);
    let manifest = this._getProjectManifestURL(project);
    this._getTargetForApp(manifest).then((target) => {
      gDevTools.showToolbox(target,
                            null,
                            devtools.Toolbox.HostType.WINDOW).then(toolbox => {
        this.connection.once(Connection.Events.DISCONNECTED, () => {
          toolbox.destroy();
        });
      });
    }, console.error);
  },


  reveal: function(location) {
    let project = AppProjects.get(location);
    if (project.type == "packaged") {
      let projectFolder = FileUtils.File(project.location);
      projectFolder.reveal();
    } else {
      // TODO: eventually open hosted apps in firefox
      // when permissions are correctly supported by firefox
    }
  },

  selectProject: function(location) {
    let projects = AppProjects.store.object.projects;
    let idx = 0;
    for (; idx < projects.length; idx++) {
      if (projects[idx].location == location) {
        break;
      }
    }
    if (idx == projects.length) {
      // Not found
      return;
    }

    let oldButton = document.querySelector(".project-item.selected");
    if (oldButton) {
      oldButton.classList.remove("selected");
    }
    let button = document.getElementById(location);
    button.classList.add("selected");

    let template = '{"path":"projects.' + idx + '","childSelector":"#lense-template"}';

    let lense = document.querySelector("#lense");
    lense.setAttribute("template-for", template);
    this.template._processFor(lense);
  },
}
