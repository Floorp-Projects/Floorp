/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Avoid leaks by using tmp for imports...
let tmp = {};
Cu.import("resource://gre/modules/Task.jsm", tmp);
Cu.import("resource:///modules/CustomizableUI.jsm", tmp);
let {Task, CustomizableUI} = tmp;

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function createToolbarWithPlacements(id, placements) {
  let tb = document.createElementNS(kNSXUL, "toolbar");
  tb.id = id;
  tb.setAttribute("customizable", "true");
  document.getElementById("customToolbars").appendChild(tb);
  CustomizableUI.registerArea(id, {
    type: CustomizableUI.TYPE_TOOLBAR
  });
  for (let p of placements) {
    CustomizableUI.addWidgetToArea(p, id);
  }
}

function removeCustomToolbars() {
  let customToolbarSet = document.getElementById("customToolbars");
  while (customToolbarSet.lastChild) {
    customToolbarSet.lastChild.remove();
  }
}

function resetCustomization() {
  CustomizableUI.reset();
}

function assertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  is(actualPlacements.length, expectedPlacements.length,
     "Area " + areaId + " should have the right number of items.");
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

function getAreaWidgetIds(areaId) {
  let widgetAry = CustomizableUI.getWidgetsInArea(areaId);
  return widgetAry.map(x => x.id);
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
