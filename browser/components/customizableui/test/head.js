/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Avoid leaks by using tmp for imports...
let tmp = {};
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", tmp);
Cu.import("resource://gre/modules/Task.jsm", tmp);
Cu.import("resource:///modules/CustomizableUI.jsm", tmp);
let {Promise, Task, CustomizableUI} = tmp;

let ChromeUtils = {};
let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

let {synthesizeDragStart, synthesizeDrop} = ChromeUtils;

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function createDummyXULButton(id, label) {
  let btn = document.createElementNS(kNSXUL, "toolbarbutton");
  btn.id = id;
  btn.setAttribute("label", label || id);
  btn.className = "toolbarbutton-1 chromeclass-toolbar-additional";
  window.gNavToolbox.palette.appendChild(btn);
  return btn;
}

function createToolbarWithPlacements(id, placements) {
  let tb = document.createElementNS(kNSXUL, "toolbar");
  tb.id = id;
  tb.setAttribute("customizable", "true");
  document.getElementById("customToolbars").appendChild(tb);
  CustomizableUI.registerArea(id, {
    type: CustomizableUI.AREATYPE_TOOLBAR,
    defaultPlacements: placements
  });
}

function removeCustomToolbars() {
  let customToolbarSet = document.getElementById("customToolbars");
  while (customToolbarSet.lastChild) {
    CustomizableUI.unregisterArea(customToolbarSet.lastChild.id);
    customToolbarSet.lastChild.remove();
  }
}

function resetCustomization() {
  if (document.documentElement.hasAttribute("customizing")) {
    window.gCustomizeMode.reset();
    window.gCustomizeMode.exit();
  } else {
    CustomizableUI.reset();
  }
}

function assertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  is(actualPlacements.length, expectedPlacements.length,
     "Area " + areaId + " should have " + expectedPlacements.length + " items.");
  let minItems = Math.min(expectedPlacements.length, actualPlacements.length);
  for (let i = 0; i < minItems; i++) {
    if (typeof expectedPlacements[i] == "string") {
      is(actualPlacements[i], expectedPlacements[i],
         "Item " + i + " in " + areaId + " should match expectations.");
    } else if (expectedPlacements[i] instanceof RegExp) {
      ok(expectedPlacements[i].test(actualPlacements[i]),
         "Item " + i + " (" + actualPlacements[i] + ") in " +
         areaId + " should match " + expectedPlacements[i]); 
    } else {
      ok(false, "Unknown type of expected placement passed to " +
                " assertAreaPlacements. Is your test broken?");
    }
  }
}

function assertSetsEqual(aSetA, aSetB) {
  if (aSetA.size != aSetB.size)
    ok(false, "Sets were not equal in size.");

  for (let item of aSetA) {
    if (!aSetB.has(item)) {
      ok(false, "Left set contained " + item + " which was not in right set.");
    }
  }
}

function getAreaWidgetIds(areaId) {
  let widgetAry = CustomizableUI.getWidgetsInArea(areaId);
  return widgetAry.map(x => x.id);
}

function simulateItemDrag(toDrag, target) {
  let docId = toDrag.ownerDocument.documentElement.id;
  let dragData = [[{type: 'text/toolbarwrapper-id/' + docId,
                    data: {id: toDrag.id, width: toDrag.getBoundingClientRect().width + 'px'}}]];
  synthesizeDragStart(toDrag.parentNode, dragData);
  synthesizeDrop(target, target, dragData);
}

function endCustomizing() {
  let deferredEndCustomizing = Promise.defer();
  function onCustomizationEnds() {
    window.gNavToolbox.removeEventListener("aftercustomization", onCustomizationEnds);
    deferredEndCustomizing.resolve();
  }
  window.gNavToolbox.addEventListener("aftercustomization", onCustomizationEnds);
  window.gCustomizeMode.exit();

  let deferredLoadNewTab = Promise.defer();

  //XXXgijs so some tests depend on this tab being about:blank. Make it so.
  let newTabBrowser = window.gBrowser.selectedBrowser;
  newTabBrowser.stop();

  // If we stop early enough, this might actually be about:blank.
  if (newTabBrowser.contentDocument.location.href == "about:blank") {
    return deferredEndCustomizing.promise;
  }

  // Otherwise, make it be about:blank, and wait for that to be done.
  function onNewTabLoaded(e) {
    newTabBrowser.removeEventListener("load", onNewTabLoaded, true);
    deferredLoadNewTab.resolve();
  }
  newTabBrowser.addEventListener("load", onNewTabLoaded, true);
  newTabBrowser.contentDocument.location.replace("about:blank");

  return Promise.all(deferredEndCustomizing.promise, deferredLoadNewTab.promise);
}

function startCustomizing() {
  let deferred = Promise.defer();
  function onCustomizing() {
    window.gNavToolbox.removeEventListener("customizationready", onCustomizing);
    deferred.resolve();
  }
  window.gNavToolbox.addEventListener("customizationready", onCustomizing);
  window.gCustomizeMode.enter();
  return deferred.promise;
}

function createToolbarItem(aProperties) {
  let node = document.createElement("toolbaritem");
  for (let attribute in aProperties) {
    node.setAttribute(attribute, aProperties[attribute]);
  }
  return node;
}

function addToPalette(...aNodes) {
  for (let node of aNodes) {
    gNavToolbox.palette.appendChild(node);
  }
}

function testRunner(testAry) {
  for (let test of testAry) {
    info(test.desc);

    if (test.setup)
      yield test.setup();

    info("Running test");
    yield test.run();
    info("Cleanup");
    if (test.teardown)
      yield test.teardown();
  }
}

function runTests(testAry) {
  Task.spawn(testRunner(gTests)).then(finish, ex => {
    ok(false, "Unexpected exception: " + ex);
    finish();
  });
}
