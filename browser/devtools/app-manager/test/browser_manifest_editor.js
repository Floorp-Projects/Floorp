/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {Services} = Cu.import("resource://gre/modules/Services.jsm");

const MANIFEST_EDITOR_ENABLED = "devtools.appmanager.manifestEditor.enabled";


let gManifestWindow, gManifestEditor;

function test() {
  waitForExplicitFinish();

  Task.spawn(function() {
    Services.prefs.setBoolPref(MANIFEST_EDITOR_ENABLED, true);
    let tab = yield openAppManager();
    yield selectProjectsPanel();
    yield addSamplePackagedApp();
    yield showSampleProjectDetails();

    gManifestWindow = getManifestWindow();
    gManifestEditor = getProjectsWindow().UI.manifestEditor;
    yield changeManifestValue("name", "the best app");
    yield addNewManifestProperty("developer", "foo", "bar");
    gManifestWindow = null;
    gManifestEditor = null;

    yield removeSamplePackagedApp();
    yield removeTab(tab);
    Services.prefs.setBoolPref(MANIFEST_EDITOR_ENABLED, false);
    finish();
  });
}

// Wait until the animation from commitHierarchy has completed
function waitForUpdate() {
  return waitForTime(gManifestEditor.editor.lazyEmptyDelay + 1);
}

function changeManifestValue(key, value) {
  return Task.spawn(function() {
    let propElem = gManifestWindow.document
                   .querySelector("[id ^= '" + key + "']");
    is(propElem.querySelector(".name").value, key,
       "Key doesn't match expected value");

    let valueElem = propElem.querySelector(".value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, valueElem, gManifestWindow);

    let valueInput = propElem.querySelector(".element-value-input");
    valueInput.value = '"' + value + '"';
    EventUtils.sendKey("RETURN", gManifestWindow);

    yield waitForUpdate();
    // Elements have all been replaced, re-select them
    propElem = gManifestWindow.document.querySelector("[id ^= '" + key + "']");
    valueElem = propElem.querySelector(".value");
    is(valueElem.value, '"' + value + '"',
       "Value doesn't match expected value");

    is(gManifestEditor.manifest[key], value,
       "Manifest doesn't contain expected value");
  });
}

function addNewManifestProperty(parent, key, value) {
  return Task.spawn(function() {
    let parentElem = gManifestWindow.document
                     .querySelector("[id ^= '" + parent + "']");
    ok(parentElem,
      "Found parent element");
    let addPropertyElem = parentElem
                          .querySelector(".variables-view-add-property");
    ok(addPropertyElem,
      "Found add-property button");

    EventUtils.sendMouseEvent({ type: "mousedown" }, addPropertyElem, gManifestWindow);

    let nameInput = parentElem.querySelector(".element-name-input");
    nameInput.value = key;
    EventUtils.sendKey("TAB", gManifestWindow);

    let valueInput = parentElem.querySelector(".element-value-input");
    valueInput.value = '"' + value + '"';
    EventUtils.sendKey("RETURN", gManifestWindow);

    yield waitForUpdate();

    let newElem = gManifestWindow.document.querySelector("[id ^= '" + key + "']");
    let nameElem = newElem.querySelector(".name");
    is(nameElem.value, key,
       "Key doesn't match expected Key");

    ok(key in gManifestEditor.manifest[parent],
       "Manifest doesn't contain expected key");

    let valueElem = newElem.querySelector(".value");
    is(valueElem.value, '"' + value + '"',
       "Value doesn't match expected value");

    is(gManifestEditor.manifest[parent][key], value,
       "Manifest doesn't contain expected value");
  });
}
