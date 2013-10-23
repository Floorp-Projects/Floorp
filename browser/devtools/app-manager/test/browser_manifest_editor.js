/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {Services} = Cu.import("resource://gre/modules/Services.jsm");

const MANIFEST_EDITOR_ENABLED = "devtools.appmanager.manifestEditor.enabled";

function test() {
  waitForExplicitFinish();

  Task.spawn(function() {
    Services.prefs.setBoolPref(MANIFEST_EDITOR_ENABLED, true);
    let tab = yield openAppManager();
    yield selectProjectsPanel();
    yield addSamplePackagedApp();
    yield showSampleProjectDetails();
    yield changeManifestValue("name", "the best app");
    yield removeSamplePackagedApp();
    yield removeTab(tab);
    Services.prefs.setBoolPref(MANIFEST_EDITOR_ENABLED, false);
    finish();
  });
}

function changeManifestValue(key, value) {
  return Task.spawn(function() {
    let manifestWindow = getManifestWindow();
    let manifestEditor = getProjectsWindow().UI.manifestEditor;

    let propElem = manifestWindow.document
                   .querySelector("[id ^= '" + key + "']");
    is(propElem.querySelector(".name").value, key,
       "Key doesn't match expected value");

    let valueElem = propElem.querySelector(".value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, valueElem, manifestWindow);

    let valueInput = propElem.querySelector(".element-value-input");
    valueInput.value = '"' + value + '"';
    EventUtils.sendKey("RETURN", manifestWindow);

    // Wait until the animation from commitHierarchy has completed
    yield waitForTime(manifestEditor.editor.lazyEmptyDelay + 1);
    // Elements have all been replaced, re-select them
    propElem = manifestWindow.document.querySelector("[id ^= '" + key + "']");
    valueElem = propElem.querySelector(".value");
    is(valueElem.value, '"' + value + '"',
       "Value doesn't match expected value");

    is(manifestEditor.manifest[key], value,
       "Manifest doesn't contain expected value");
  });
}
