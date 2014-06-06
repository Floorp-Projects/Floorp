/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that options can be changed without resetting the whole
// editor.
let test = asyncTest(function*() {

  let TEMP_PATH = buildTempDirectoryStructure();
  let projecteditor = yield addProjectEditorTab();

  let resourceBeenAdded = promise.defer();
  projecteditor.project.once("resource-added", () => {
    info ("A resource has been added");
    resourceBeenAdded.resolve();
  });

  info ("About to set project to: " + TEMP_PATH);
  yield projecteditor.setProjectToAppPath(TEMP_PATH, {
    name: "Test",
    iconUrl: "chrome://browser/skin/devtools/tool-options.svg",
    projectOverviewURL: SAMPLE_WEBAPP_URL
  });

  info ("Making sure a resource has been added before continuing");
  yield resourceBeenAdded.promise;

  info ("From now on, if a resource is added it should fail");
  projecteditor.project.on("resource-added", failIfResourceAdded);

  info ("Getting ahold and validating the project header DOM");
  let header = projecteditor.document.querySelector(".entry-group-title");
  let image = header.querySelector(".project-image");
  let nameLabel = header.querySelector(".project-name-label");
  let statusElement = header.querySelector(".project-status");
  is (statusElement.getAttribute("status"), "unknown", "The status starts out as unknown.");
  is (nameLabel.textContent, "Test", "The name label has been set correctly");
  is (image.getAttribute("src"), "chrome://browser/skin/devtools/tool-options.svg", "The icon has been set correctly");

  info ("About to set project with new options.");
  yield projecteditor.setProjectToAppPath(TEMP_PATH, {
    name: "Test2",
    iconUrl: "chrome://browser/skin/devtools/tool-inspector.svg",
    projectOverviewURL: SAMPLE_WEBAPP_URL,
    validationStatus: "error"
  });

  ok (!nameLabel.parentNode, "The old elements have been removed");

  info ("Getting ahold of and validating the project header DOM");
  let image = header.querySelector(".project-image");
  let nameLabel = header.querySelector(".project-name-label");
  let statusElement = header.querySelector(".project-status");
  is (statusElement.getAttribute("status"), "error", "The status has been set correctly.");
  is (nameLabel.textContent, "Test2", "The name label has been set correctly");
  is (image.getAttribute("src"), "chrome://browser/skin/devtools/tool-inspector.svg", "The icon has been set correctly");

  info ("About to set project with new options.");
  yield projecteditor.setProjectToAppPath(TEMP_PATH, {
    name: "Test3",
    iconUrl: "chrome://browser/skin/devtools/tool-webconsole.svg",
    projectOverviewURL: SAMPLE_WEBAPP_URL,
    validationStatus: "warning"
  });

  ok (!nameLabel.parentNode, "The old elements have been removed");

  info ("Getting ahold of and validating the project header DOM");
  let image = header.querySelector(".project-image");
  let nameLabel = header.querySelector(".project-name-label");
  let statusElement = header.querySelector(".project-status");
  is (statusElement.getAttribute("status"), "warning", "The status has been set correctly.");
  is (nameLabel.textContent, "Test3", "The name label has been set correctly");
  is (image.getAttribute("src"), "chrome://browser/skin/devtools/tool-webconsole.svg", "The icon has been set correctly");

  info ("About to set project with new options.");
  yield projecteditor.setProjectToAppPath(TEMP_PATH, {
    name: "Test4",
    iconUrl: "chrome://browser/skin/devtools/tool-debugger.svg",
    projectOverviewURL: SAMPLE_WEBAPP_URL,
    validationStatus: "valid"
  });

  ok (!nameLabel.parentNode, "The old elements have been removed");

  info ("Getting ahold of and validating the project header DOM");
  let image = header.querySelector(".project-image");
  let nameLabel = header.querySelector(".project-name-label");
  let statusElement = header.querySelector(".project-status");
  is (statusElement.getAttribute("status"), "valid", "The status has been set correctly.");
  is (nameLabel.textContent, "Test4", "The name label has been set correctly");
  is (image.getAttribute("src"), "chrome://browser/skin/devtools/tool-debugger.svg", "The icon has been set correctly");

  info ("Test finished, cleaning up");
  projecteditor.project.off("resource-added", failIfResourceAdded);
});

function failIfResourceAdded() {
  ok (false, "A resource has been added, but it shouldn't have been");
}
