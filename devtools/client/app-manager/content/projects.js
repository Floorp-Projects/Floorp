/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;
Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
const {ConnectionManager, Connection} = require("devtools/shared/client/connection-manager");
const {AppProjects} = require("devtools/client/app-manager/app-projects");
const {AppValidator} = require("devtools/client/app-manager/app-validator");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {installHosted, installPackaged, getTargetForApp,
       reloadApp, launchApp, closeApp} = require("devtools/shared/apps/app-actor-front");
const {EventEmitter} = Cu.import("resource://gre/modules/devtools/shared/event-emitter.js");

const promise = require("devtools/shared/deprecated-sync-thenables");

const MANIFEST_EDITOR_ENABLED = "devtools.appmanager.manifestEditor.enabled";

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
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  UI.destroy();
});

var UI = {
  isReady: false,

  onload: function() {
    if (Services.prefs.getBoolPref(MANIFEST_EDITOR_ENABLED)) {
      document.querySelector("#lense").setAttribute("manifest-editable", "");
    }

    this.template = new Template(document.body, AppProjects.store, Utils.l10n);
    this.template.start();

    AppProjects.load().then(() => {
      AppProjects.store.object.projects.forEach(UI.validate);
      this.isReady = true;
      this.emit("ready");
    });
  },

  destroy: function() {
    if (this.connection) {
      this.connection.off(Connection.Events.STATUS_CHANGED, this._onConnectionStatusChange);
    }
    this.template.destroy();
  },

  onNewConnection: function() {
    this.connection.on(Connection.Events.STATUS_CHANGED, this._onConnectionStatusChange);
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

  get connected() { return !!this.listTabsResponse; },

  _selectFolder: function() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, Utils.l10n("project.filePickerTitle"), Ci.nsIFilePicker.modeGetFolder);
    let res = fp.show();
    if (res != Ci.nsIFilePicker.returnCancel)
      return fp.file;
    return null;
  },

  addPackaged: function(folder) {
    if (!folder) {
      folder = this._selectFolder();
    }
    if (!folder)
      return;
    return AppProjects.addPackaged(folder)
                      .then(function (project) {
                        UI.validate(project);
                        UI.selectProject(project.location);
                      });
  },

  addHosted: function() {
    let form = document.querySelector("#new-hosted-project-wrapper");
    if (!form.checkValidity())
      return;

    let urlInput = document.querySelector("#url-input");
    let manifestURL = urlInput.value;
    return AppProjects.addHosted(manifestURL)
                      .then(function (project) {
                        UI.validate(project);
                        UI.selectProject(project.location);
                      });
  },

  _getLocalIconURL: function(project, manifest) {
    let icon;
    if (manifest.icons) {
      let size = Object.keys(manifest.icons).sort((a, b) => b - a)[0];
      if (size) {
        icon = manifest.icons[size];
      }
    }
    if (!icon)
      return "chrome://devtools/skin/themes/app-manager/images/default-app-icon.png";
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

        if (project.warningsCount && project.errorsCount) {
          project.validationStatus = "error warning";
        }

      });

  },

  update: function(button, location) {
    button.disabled = true;
    let project = AppProjects.get(location);

    // Update the manifest editor view, in case the manifest was modified
    // outside of the app manager.  This can happen in parallel with the other
    // steps.
    this._showManifestEditor(project);

    this.validate(project)
        .then(() => {
           // Install the app to the device if we are connected,
           // and there is no error
           if (project.errorsCount == 0 && this.connected) {
             return this.install(project);
           }
         })
        .then(() => {
           button.disabled = false;
           // Finally try to reload the app if it is already opened
           if (this.connected) {
             this.reload(project);
           }
        },
        (res) => {
          button.disabled = false;
          let message = res.error + ": " + res.message;
          alert(message);
          this.connection.log(message);
        });
  },

  saveManifest: function(button) {
    button.disabled = true;
    this.manifestEditor.save().then(() => button.disabled = false);
  },

  reload: function (project) {
    if (!this.connected) {
      return promise.reject();
    }
    return reloadApp(this.connection.client,
              this.listTabsResponse.webappsActor,
              this._getProjectManifestURL(project)).
      then(() => {
        this.connection.log("App reloaded");
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
    if (!this.connected) {
      return promise.reject();
    }
    this.connection.log("Installing the " + project.manifest.name + " app...");
    let installPromise;
    if (project.type == "packaged") {
      installPromise = installPackaged(this.connection.client, this.listTabsResponse.webappsActor, project.location, project.packagedAppOrigin)
        .then(({ appId }) => {
          // If the packaged app specified a custom origin override,
          // we need to update the local project origin
          project.packagedAppOrigin = appId;
          // And ensure the indexed db on disk is also updated
          AppProjects.update(project);
        });
    } else {
      let manifestURLObject = Services.io.newURI(project.location, null, null);
      let origin = Services.io.newURI(manifestURLObject.prePath, null, null);
      let appId = origin.host;
      let metadata = {
        origin: origin.spec,
        manifestURL: project.location
      };
      installPromise = installHosted(this.connection.client, this.listTabsResponse.webappsActor, appId, metadata, project.manifest);
    }

    installPromise.then(() => {
      this.connection.log("Install completed.");
    }, () => {
      this.connection.log("Install failed.");
    });

    return installPromise;
  },

  start: function(project) {
    if (!this.connected) {
      return promise.reject();
    }
    let manifestURL = this._getProjectManifestURL(project);
    return launchApp(this.connection.client,
                     this.listTabsResponse.webappsActor,
                     manifestURL);
  },

  stop: function(location) {
    if (!this.connected) {
      return promise.reject();
    }
    let project = AppProjects.get(location);
    let manifestURL = this._getProjectManifestURL(project);
    return closeApp(this.connection.client,
                    this.listTabsResponse.webappsActor,
                    manifestURL);
  },

  debug: function(button, location) {
    if (!this.connected) {
      return promise.reject();
    }
    button.disabled = true;
    let project = AppProjects.get(location);

    let onFailedToStart = (error) => {
      // If not installed, install and open it
      if (error == "NO_SUCH_APP") {
        return this.install(project);
      } else {
        throw error;
      }
    };
    let onStarted = () => {
      // Once we asked the app to launch, the app isn't necessary completely loaded.
      // launch request only ask the app to launch and immediatly returns.
      // We have to keep trying to get app tab actors required to create its target.
      let deferred = promise.defer();
      let loop = (count) => {
        // Ensure not looping for ever
        if (count >= 100) {
          deferred.reject("Unable to connect to the app");
          return;
        }
        // Also, in case the app wasn't installed yet, we also have to keep asking the
        // app to launch, as launch request made right after install may race.
        this.start(project);
        getTargetForApp(
          this.connection.client,
          this.listTabsResponse.webappsActor,
          this._getProjectManifestURL(project)).
            then(deferred.resolve,
                 (err) => {
                   if (err == "appNotFound")
                     setTimeout(loop, 500, count + 1);
                   else
                     deferred.reject(err);
                 });
      };
      loop(0);
      return deferred.promise;
    };

    // First try to open the app
    this.start(project)
        .then(null, onFailedToStart)
        .then(onStarted)
        .then((target) =>
          top.UI.openAndShowToolboxForTarget(target,
                                             project.manifest.name,
                                             project.icon))
        .then(() => {
           // And only when the toolbox is opened, release the button
           button.disabled = false;
         },
         (err) => {
           button.disabled = false;
           let message = err.error ? err.error + ": " + err.message : String(err);
           alert(message);
           this.connection.log(message);
         });
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

    let project = projects[idx];
    this._showManifestEditor(project).then(() => this.emit("project-selected"));
  },

  _showManifestEditor: function(project) {
    if (this.manifestEditor) {
      this.manifestEditor.destroy();
    }
    let editorContainer = document.querySelector("#lense .manifest-editor");
    this.manifestEditor = new ManifestEditor(project);
    return this.manifestEditor.show(editorContainer);
  }
};

// This must be bound immediately, as it might be used via the message listener
// before UI.onload() has been called.
UI._onConnectionStatusChange = UI._onConnectionStatusChange.bind(UI);

EventEmitter.decorate(UI);
