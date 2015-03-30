/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {AppManager} = require("devtools/webide/app-manager");
const ProjectList = require("devtools/webide/project-list");

let projectList = new ProjectList(window, window.parent);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  AppManager.on("app-manager-update", onAppManagerUpdate);
  document.getElementById("new-app").onclick = CreateNewApp;
  document.getElementById("hosted-app").onclick = ImportHostedApp;
  document.getElementById("packaged-app").onclick = ImportPackagedApp;
  projectList.update();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  projectList = null;
  AppManager.off("app-manager-update", onAppManagerUpdate);
});

function onAppManagerUpdate(event, what, details) {
  switch (what) {
    case "runtime-global-actors":
    case "runtime-targets":
    case "project-validated":
    case "project-removed":
    case "project":
      projectList.update(details);
      break;
  }
}

function CreateNewApp() {
  projectList.newApp();
}

function ImportHostedApp() {
  projectList.importHostedApp();
}

function ImportPackagedApp() {
  projectList.importPackagedApp();
}
