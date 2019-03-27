/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
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
  const projectNameNode = document.querySelector("#project-name");
  projectNameNode.addEventListener("input", canValidate, true);
  getTemplatesJSON();
  document.addEventListener("dialogaccept", doOK);
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
    const templatelistNode = document.querySelector("#templatelist");
    templatelistNode.innerHTML = "";
    for (const template of list) {
      const richlistitemNode = document.createElement("richlistitem");
      const imageNode = document.createElement("image");
      imageNode.setAttribute("src", template.icon);
      const labelNode = document.createElement("label");
      labelNode.setAttribute("value", template.name);
      const descriptionNode = document.createElement("description");
      descriptionNode.textContent = template.description;
      const vboxNode = document.createElement("vbox");
      vboxNode.setAttribute("flex", "1");
      richlistitemNode.appendChild(imageNode);
      vboxNode.appendChild(labelNode);
      vboxNode.appendChild(descriptionNode);
      richlistitemNode.appendChild(vboxNode);
      templatelistNode.appendChild(richlistitemNode);
    }
    templatelistNode.selectedIndex = 0;

    /* Chrome mochitest support */
    const testOptions = window.arguments[0].testOptions;
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
  const projectNameNode = document.querySelector("#project-name");
  const dialogNode = document.querySelector("dialog");
  if (projectNameNode.value.length > 0) {
    dialogNode.removeAttribute("buttondisabledaccept");
  } else {
    dialogNode.setAttribute("buttondisabledaccept", "true");
  }
}

function doOK(event) {
  const projectName = document.querySelector("#project-name").value;

  if (!projectName) {
    console.error("No project name");
    event.preventDefault();
    return;
  }

  if (!gTemplateList) {
    console.error("No template index");
    event.preventDefault();
    return;
  }

  const templatelistNode = document.querySelector("#templatelist");
  if (templatelistNode.selectedIndex < 0) {
    console.error("No template selected");
    event.preventDefault();
    return;
  }

  /* Chrome mochitest support */
  const promise = new Promise((resolve, reject) => {
    const testOptions = window.arguments[0].testOptions;
    if (testOptions) {
      resolve(testOptions.folder);
    } else {
      const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
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

  const bail = (e) => {
    console.error(e);
    window.close();
  };

  promise.then(folder => {
    // Create subfolder with fs-friendly name of project
    const subfolder = projectName.replace(/[\\/:*?"<>|]/g, "").toLowerCase();
    const win = Services.wm.getMostRecentWindow("devtools:webide");
    folder.append(subfolder);

    try {
      folder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    } catch (e) {
      win.UI.reportError("error_folderCreationFailed");
      window.close();
      return;
    }

    // Download boilerplate zip
    const template = gTemplateList[templatelistNode.selectedIndex];
    const source = template.file;
    const target = folder.clone();
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

  event.preventDefault();
}
