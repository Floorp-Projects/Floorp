/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const ProjectList = require("devtools/webide/project-list");

let projectList = new ProjectList(window, window.parent);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad, true);
  document.getElementById("new-app").onclick = CreateNewApp;
  document.getElementById("hosted-app").onclick = ImportHostedApp;
  document.getElementById("packaged-app").onclick = ImportPackagedApp;
  projectList.update();
  projectList.updateCommands();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  projectList.destroy();
});

function CreateNewApp() {
  projectList.newApp();
}

function ImportHostedApp() {
  projectList.importHostedApp();
}

function ImportPackagedApp() {
  projectList.importPackagedApp();
}
