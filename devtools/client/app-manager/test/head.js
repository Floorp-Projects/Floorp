/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const {Promise: promise} =
  Cu.import("resource://devtools/shared/deprecated-sync-thenables.js", {});
const {require} =
  Cu.import("resource://devtools/shared/Loader.jsm", {});

const {AppProjects} = require("devtools/client/app-manager/app-projects");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const APP_MANAGER_URL = "about:app-manager";
const TEST_BASE =
  "chrome://mochitests/content/browser/devtools/client/app-manager/test/";
const HOSTED_APP_MANIFEST = TEST_BASE + "hosted_app.manifest";

const PACKAGED_APP_DIR_PATH = getTestFilePath(".");

DevToolsUtils.testing = true;
SimpleTest.registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function addTab(url, targetWindow = window) {
  info("Adding tab: " + url);

  let deferred = promise.defer();
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(url);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + url);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(tab, targetWindow = window) {
  info("Removing tab.");

  let deferred = promise.defer();
  let targetBrowser = targetWindow.gBrowser;
  let tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  targetBrowser.removeTab(tab);

  return deferred.promise;
}

function openAppManager() {
  return addTab(APP_MANAGER_URL);
}

function addSampleHostedApp() {
  info("Adding sample hosted app");
  let projectsWindow = getProjectsWindow();
  let projectsDocument = projectsWindow.document;
  let url = projectsDocument.querySelector("#url-input");
  url.value = HOSTED_APP_MANIFEST;
  return projectsWindow.UI.addHosted();
}

function removeSampleHostedApp() {
  info("Removing sample hosted app");
  return AppProjects.remove(HOSTED_APP_MANIFEST);
}

function addSamplePackagedApp() {
  info("Adding sample packaged app");
  let appDir = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
  appDir.initWithPath(PACKAGED_APP_DIR_PATH);
  return getProjectsWindow().UI.addPackaged(appDir);
}

function removeSamplePackagedApp() {
  info("Removing sample packaged app");
  return AppProjects.remove(PACKAGED_APP_DIR_PATH);
}

function getProjectsWindow() {
  return content.document.querySelector(".projects-panel").contentWindow;
}

function getManifestWindow() {
  return getProjectsWindow().document.querySelector(".variables-view")
         .contentWindow;
}

function waitForProjectsPanel(deferred = promise.defer()) {
  info("Wait for projects panel");

  let projectsWindow = getProjectsWindow();
  let projectsUI = projectsWindow.UI;
  if (!projectsUI) {
    info("projectsUI false");
    projectsWindow.addEventListener("load", function onLoad() {
      info("got load event");
      projectsWindow.removeEventListener("load", onLoad);
      waitForProjectsPanel(deferred);
    });
    return deferred.promise;
  }

  if (projectsUI.isReady) {
    info("projectsUI ready");
    deferred.resolve();
    return deferred.promise;
  }

  info("projectsUI not ready");
  projectsUI.once("ready", deferred.resolve);
  return deferred.promise;
}

function selectProjectsPanel() {
  return Task.spawn(function*() {
    let projectsButton = content.document.querySelector(".projects-button");
    EventUtils.sendMouseEvent({ type: "click" }, projectsButton, content);

    yield waitForProjectsPanel();
  });
}

function waitForProjectSelection() {
  info("Wait for project selection");

  let deferred = promise.defer();
  getProjectsWindow().UI.once("project-selected", deferred.resolve);
  return deferred.promise;
}

function selectFirstProject() {
  return Task.spawn(function*() {
    let projectsFrame = content.document.querySelector(".projects-panel");
    let projectsWindow = projectsFrame.contentWindow;
    let projectsDoc = projectsWindow.document;
    let projectItem = projectsDoc.querySelector(".project-item");
    EventUtils.sendMouseEvent({ type: "click" }, projectItem, projectsWindow);

    yield waitForProjectSelection();
  });
}

function showSampleProjectDetails() {
  return Task.spawn(function*() {
    yield selectProjectsPanel();
    yield selectFirstProject();
  });
}

function waitForTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}

function waitForTime(aDelay) {
  let deferred = promise.defer();
  setTimeout(deferred.resolve, aDelay);
  return deferred.promise;
}
