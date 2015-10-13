/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Cu = Components.utils;
var Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ZipUtils", "resource://gre/modules/ZipUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {AppProjects} = require("devtools/client/app-manager/app-projects");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const {getJSON} = require("devtools/client/shared/getjson");

const TEMPLATES_URL = "devtools.webide.templatesURL";

var gTemplateList = null;

// See bug 989619
console.log = console.log.bind(console);
console.warn = console.warn.bind(console);
console.error = console.error.bind(console);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  let projectNameNode = document.querySelector("#project-name");
  projectNameNode.addEventListener("input", canValidate, true);
  getTemplatesJSON();
}, true);

function getTemplatesJSON() {
  getJSON(TEMPLATES_URL).then(list => {
    if (!Array.isArray(list)) {
      throw new Error("JSON response not an array");
    }
    if (list.length == 0) {
      throw new Error("JSON response is an empty array");
    }
    gTemplateList = list;
    let templatelistNode = document.querySelector("#templatelist");
    templatelistNode.innerHTML = "";
    for (let template of list) {
      let richlistitemNode = document.createElement("richlistitem");
      let imageNode = document.createElement("image");
      imageNode.setAttribute("src", template.icon);
      let labelNode = document.createElement("label");
      labelNode.setAttribute("value", template.name);
      let descriptionNode = document.createElement("description");
      descriptionNode.textContent = template.description;
      let vboxNode = document.createElement("vbox");
      vboxNode.setAttribute("flex", "1");
      richlistitemNode.appendChild(imageNode);
      vboxNode.appendChild(labelNode);
      vboxNode.appendChild(descriptionNode);
      richlistitemNode.appendChild(vboxNode);
      templatelistNode.appendChild(richlistitemNode);
    }
    templatelistNode.selectedIndex = 0;

    /* Chrome mochitest support */
    let testOptions = window.arguments[0].testOptions;
    if (testOptions) {
      templatelistNode.selectedIndex = testOptions.index;
      document.querySelector("#project-name").value = testOptions.name;
      doOK();
    }
  }, (e) => {
    failAndBail("Can't download app templates: " + e);
  });
}

function failAndBail(msg) {
  let promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci.nsIPromptService);
  promptService.alert(window, "error", msg);
  window.close();
}

function canValidate() {
  let projectNameNode = document.querySelector("#project-name");
  let dialogNode = document.querySelector("dialog");
  if (projectNameNode.value.length > 0) {
    dialogNode.removeAttribute("buttondisabledaccept");
  } else {
    dialogNode.setAttribute("buttondisabledaccept", "true");
  }
}

function doOK() {
  let projectName = document.querySelector("#project-name").value;

  if (!projectName) {
    console.error("No project name");
    return false;
  }

  if (!gTemplateList) {
    console.error("No template index");
    return false;
  }

  let templatelistNode = document.querySelector("#templatelist");
  if (templatelistNode.selectedIndex < 0) {
    console.error("No template selected");
    return false;
  }

  let folder;

  /* Chrome mochitest support */
  let testOptions = window.arguments[0].testOptions;
  if (testOptions) {
    folder = testOptions.folder;
  } else {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, "Select directory where to create app directory", Ci.nsIFilePicker.modeGetFolder);
    let res = fp.show();
    if (res == Ci.nsIFilePicker.returnCancel) {
      console.error("No directory selected");
      return false;
    }
    folder = fp.file;
  }

  // Create subfolder with fs-friendly name of project
  let subfolder = projectName.replace(/[\\/:*?"<>|]/g, '').toLowerCase();
  let win = Services.wm.getMostRecentWindow("devtools:webide");
  folder.append(subfolder);

  try {
    folder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  } catch(e) {
    win.UI.reportError("error_folderCreationFailed");
    window.close();
    return false;
  }

  // Download boilerplate zip
  let template = gTemplateList[templatelistNode.selectedIndex];
  let source = template.file;
  let target = folder.clone();
  target.append(subfolder + ".zip");

  let bail = (e) => {
    console.error(e);
    window.close();
  };

  Downloads.fetch(source, target).then(() => {
    ZipUtils.extractFiles(target, folder);
    target.remove(false);
    AppProjects.addPackaged(folder).then((project) => {
      window.arguments[0].location = project.location;
      AppManager.validateAndUpdateProject(project).then(() => {
        if (project.manifest) {
          project.manifest.name = projectName;
          AppManager.writeManifest(project).then(() => {
            AppManager.validateAndUpdateProject(project).then(
              () => {window.close()}, bail)
          }, bail)
        } else {
          bail("Manifest not found");
        }
      }, bail)
    }, bail)
  }, bail);

  return false;
}
