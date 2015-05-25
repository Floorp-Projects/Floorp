/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {Services} = Cu.import("resource://gre/modules/Services.jsm");

const MANIFEST_EDITOR_ENABLED = "devtools.appmanager.manifestEditor.enabled";

let gManifestWindow, gManifestEditor;

function test() {
  waitForExplicitFinish();

  Task.spawn(function*() {
    Services.prefs.setBoolPref(MANIFEST_EDITOR_ENABLED, true);
    let tab = yield openAppManager();
    yield selectProjectsPanel();
    yield addSamplePackagedApp();
    yield showSampleProjectDetails();

    gManifestWindow = getManifestWindow();
    gManifestEditor = getProjectsWindow().UI.manifestEditor;
    yield changeManifestValue("name", "the best app");
    yield changeManifestValueBad("name", "the worst app");
    yield addNewManifestProperty("developer", "foo", "bar");

    // add duplicate property in the same parent doesn't create duplicates
    yield addNewManifestProperty("developer", "foo", "bar2");

    // add propery with same key in other parent is allowed
    yield addNewManifestProperty("tester", "foo", "new");

    yield addNewManifestPropertyBad("developer", "blob", "bob");
    yield removeManifestProperty("developer", "foo");
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
  return Task.spawn(function*() {
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

function changeManifestValueBad(key, value) {
  return Task.spawn(function*() {
    let propElem = gManifestWindow.document
                   .querySelector("[id ^= '" + key + "']");
    is(propElem.querySelector(".name").value, key,
       "Key doesn't match expected value");

    let valueElem = propElem.querySelector(".value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, valueElem, gManifestWindow);

    let valueInput = propElem.querySelector(".element-value-input");
    // Leaving out quotes will result in an error, so no change should be made.
    valueInput.value = value;
    EventUtils.sendKey("RETURN", gManifestWindow);

    yield waitForUpdate();
    // Elements have all been replaced, re-select them
    propElem = gManifestWindow.document.querySelector("[id ^= '" + key + "']");
    valueElem = propElem.querySelector(".value");
    isnot(valueElem.value, '"' + value + '"',
       "Value was changed, but it should not have been");

    isnot(gManifestEditor.manifest[key], value,
       "Manifest was changed, but it should not have been");
  });
}

function addNewManifestProperty(parent, key, value) {
  info("Adding new property - parent: " + parent + "; key: " + key + "; value: " + value + "\n\n");
  return Task.spawn(function*() {
    let parentElem = gManifestWindow.document
                     .querySelector("[id ^= '" + parent + "']");
    ok(parentElem, "Found parent element: " + parentElem.id);

    let addPropertyElem = parentElem.querySelector(".variables-view-add-property");
    ok(addPropertyElem, "Found add-property button");

    EventUtils.sendMouseEvent({ type: "mousedown" }, addPropertyElem, gManifestWindow);

    let nameInput = parentElem.querySelector(".element-name-input");
    nameInput.value = key;
    EventUtils.sendKey("TAB", gManifestWindow);

    let valueInput = parentElem.querySelector(".element-value-input");
    valueInput.value = '"' + value + '"';
    EventUtils.sendKey("RETURN", gManifestWindow);

    yield waitForUpdate();

    parentElem = gManifestWindow.document.querySelector("[id ^= '" + parent + "']");
    let elems = parentElem.querySelectorAll("[id ^= '" + key + "']");
    is(elems.length, 1, "No duplicate property is added");

    let newElem = elems[0];
    let nameElem = newElem.querySelector(".name");
    is(nameElem.value, key, "Key doesn't match expected Key");

    ok(key in gManifestEditor.manifest[parent],
       "Manifest doesn't contain expected key");

    let valueElem = newElem.querySelector(".value");
    is(valueElem.value, '"' + value + '"',
       "Value doesn't match expected value");

    is(gManifestEditor.manifest[parent][key], value,
       "Manifest doesn't contain expected value");
  });
}

function addNewManifestPropertyBad(parent, key, value) {
  return Task.spawn(function*() {
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
    // Leaving out quotes will result in an error, so no change should be made.
    valueInput.value = value;
    EventUtils.sendKey("RETURN", gManifestWindow);

    yield waitForUpdate();

    let newElem = gManifestWindow.document.querySelector("[id ^= '" + key + "']");
    ok(!newElem, "Key was added, but it should not have been");
    ok(!(key in gManifestEditor.manifest[parent]),
       "Manifest contains key, but it should not");
  });
}

function removeManifestProperty(parent, key) {
  info("*** Remove property test ***");

  return Task.spawn(function*() {
    let parentElem = gManifestWindow.document
                     .querySelector("[id ^= '" + parent + "']");
    ok(parentElem, "Found parent element");

    let keyExists = key in gManifestEditor.manifest[parent];
    ok(keyExists,
       "The manifest contains the key under the expected parent");

    let newElem = gManifestWindow.document.querySelector("[id ^= '" + key + "']");
    let removePropertyButton = newElem.querySelector(".variables-view-delete");
    ok(removePropertyButton, "The remove property button was found");
    removePropertyButton.click();

    yield waitForUpdate();

    ok(!(key in gManifestEditor.manifest[parent]), "Property was successfully removed");
  });
}
