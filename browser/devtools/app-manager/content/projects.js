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
const {installHosted, installPackaged, getTargetForApp} = require("devtools/app-actor-front");

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

    AppProjects.load().then(() => {
      AppProjects.store.object.projects.forEach(UI.validate);
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
    return validation.validate()
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

  update: function(button, location) {
    button.disabled = true;
    let project = AppProjects.get(location);
    this.validate(project)
        .then(() => {
           // Install the app to the device if we are connected,
           // and there is no error
           if (project.errorsCount == 0 && this.listTabsResponse) {
             return this.install(project);
           }
         })
        .then(
         () => {
           button.disabled = false;
         },
         (res) => {
           button.disabled = false;
           let message = res.error + ": " + res.message;
           alert(message);
           this.connection.log(message);
         });
  },

  remove: function(location, event) {
    if (event) {
      // We don't want the "click" event to be propagated to the project item.
      // That would trigger `selectProject()`.
      event.stopPropagation();
    }

    let item = document.getElementById(location);

    let toSelect = document.querySelector(".project-item.selected");
    toSelect = toSelect ? toSelect.id : "";

    if (toSelect == location) {
      toSelect = null;
      let sibling;
      if (item.previousElementSibling) {
        sibling = item.previousElementSibling;
      } else {
        sibling = item.nextElementSibling;
      }
      if (sibling && !!AppProjects.get(sibling.id)) {
        toSelect = sibling.id;
      }
    }

    AppProjects.remove(location).then(() => {
      this.selectProject(toSelect);
    });
  },

  _getProjectManifestURL: function (project) {
    if (project.type == "packaged") {
      return "app://" + project.packagedAppOrigin + "/manifest.webapp";
    } else if (project.type == "hosted") {
      return project.location;
    }
  },

  install: function(project) {
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
    return install;
  },

  start: function(project) {
    let deferred = promise.defer();
    let request = {
      to: this.listTabsResponse.webappsActor,
      type: "launch",
      manifestURL: this._getProjectManifestURL(project)
    };
    this.connection.client.request(request, (res) => {
      if (res.error)
        deferred.reject(res.error);
      else
        deferred.resolve(res);
    });
    return deferred.promise;
  },

  stop: function(location) {
    let project = AppProjects.get(location);
    let deferred = promise.defer();
    let request = {
      to: this.listTabsResponse.webappsActor,
      type: "close",
      manifestURL: this._getProjectManifestURL(project)
    };
    this.connection.client.request(request, (res) => {
      promive.resolve(res);
    });
    return deferred.promise;
  },

  debug: function(button, location) {
    button.disabled = true;
    let project = AppProjects.get(location);
    // First try to open the app
    this.start(project)
        .then(
         null,
         (error) => {
           // If not installed, install and open it
           if (error == "NO_SUCH_APP") {
             return this.install(project)
                        .then(() => this.start(project));
           } else {
             throw error;
           }
         })
        .then(() => {
           // Finally, when it's finally opened, display the toolbox
           return this.openToolbox(project)
        })
        .then(() => {
           // And only when the toolbox is opened, release the button
           button.disabled = false;
         },
         (msg) => {
           button.disabled = false;
           alert(msg);
           this.connection.log(msg);
         });
  },

  openToolbox: function(project) {
    let deferred = promise.defer();
    let manifest = this._getProjectManifestURL(project);
    getTargetForApp(this.connection.client,
                    this.listTabsResponse.webappsActor,
                    manifest).then((target) => {
      gDevTools.showToolbox(target,
                            null,
                            devtools.Toolbox.HostType.WINDOW).then(toolbox => {
        this.connection.once(Connection.Events.DISCONNECTED, () => {
          toolbox.destroy();
        });
        deferred.resolve();
      });
    }, deferred.reject);
    return deferred.promise;
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

    let oldButton = document.querySelector(".project-item.selected");
    if (oldButton) {
      oldButton.classList.remove("selected");
    }

    if (idx == projects.length) {
      // Not found. Empty lense.
      let lense = document.querySelector("#lense");
      lense.setAttribute("template-for", '{"path":"","childSelector":""}');
      this.template._processFor(lense);
      return;
    }

    let button = document.getElementById(location);
    button.classList.add("selected");

    let template = '{"path":"projects.' + idx + '","childSelector":"#lense-template"}';

    let lense = document.querySelector("#lense");
    lense.setAttribute("template-for", template);
    this.template._processFor(lense);
  },
}
