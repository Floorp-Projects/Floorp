/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {FileUtils} = require("resource://gre/modules/FileUtils.jsm");
const {AppProjects} = require("devtools/client/webide/modules/app-projects");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const {getJSON} = require("devtools/client/shared/getjson");

ChromeUtils.defineModuleGetter(this, "ZipUtils", "resource://gre/modules/ZipUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");

const TEMPLATES_URL = "devtools.webide.templatesURL";

var gTemplateList = null;

window.addEventListener("load", function() {
  let projectNameNode = document.querySelector("#project-name");
  projectNameNode.addEventListener("input", canValidate, true);
  getTemplatesJSON();
}, {capture: true, once: true});

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
  Services.prompt.alert(window, "error", msg);
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

  /* Chrome mochitest support */
  let promise = new Promise((resolve, reject) => {
    let testOptions = window.arguments[0].testOptions;
    if (testOptions) {
      resolve(testOptions.folder);
    } else {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(window, "Select directory where to create app directory", Ci.nsIFilePicker.modeGetFolder);
      fp.open(res => {
        if (res == Ci.nsIFilePicker.returnCancel) {
          console.error("No directory selected");
          reject(null);
        } else {
          resolve(fp.file);
        }
      });
    }
  });

  let bail = (e) => {
    console.error(e);
    window.close();
  };

  promise.then(folder => {
    // Create subfolder with fs-friendly name of project
    let subfolder = projectName.replace(/[\\/:*?"<>|]/g, "").toLowerCase();
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    folder.append(subfolder);

    try {
      folder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    } catch (e) {
      win.UI.reportError("error_folderCreationFailed");
      window.close();
      return;
    }

    // Download boilerplate zip
    let template = gTemplateList[templatelistNode.selectedIndex];
    let source = template.file;
    let target = folder.clone();
    target.append(subfolder + ".zip");
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
                () => {
                  window.close();
                }, bail);
            }, bail);
          } else {
            bail("Manifest not found");
          }
        }, bail);
      }, bail);
    }, bail);
  }, bail);

  return false;
}
