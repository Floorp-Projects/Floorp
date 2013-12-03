/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Avoid leaks by using tmp for imports...
let tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
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

let gAddedToolbars = new Set();

function createToolbarWithPlacements(id, placements) {
  gAddedToolbars.add(id);
  let tb = document.createElementNS(kNSXUL, "toolbar");
  tb.id = id;
  tb.setAttribute("customizable", "true");
  CustomizableUI.registerArea(id, {
    type: CustomizableUI.TYPE_TOOLBAR,
    defaultPlacements: placements
  });
  gNavToolbox.appendChild(tb);
  return tb;
}

function removeCustomToolbars() {
  CustomizableUI.reset();
  for (let toolbarId of gAddedToolbars) {
    CustomizableUI.unregisterArea(toolbarId, true);
    document.getElementById(toolbarId).remove();
  }
  gAddedToolbars.clear();
}

function resetCustomization() {
  return CustomizableUI.reset();
}

function isInWin8() {
  let sysInfo = Services.sysinfo;
  let osName = sysInfo.getProperty("name");
  let version = sysInfo.getProperty("version");

  // Windows 8 is version >= 6.2
  return osName == "Windows_NT" && version >= 6.2;
}

function addSwitchToMetroButtonInWindows8(areaPanelPlacements) {
  if (isInWin8()) {
    areaPanelPlacements.push("switch-to-metro-button");
  }
}

function assertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  placementArraysEqual(areaId, actualPlacements, expectedPlacements);
}

function placementArraysEqual(areaId, actualPlacements, expectedPlacements) {
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

function todoAssertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  let isPassing = actualPlacements.length == expectedPlacements.length;
  let minItems = Math.min(expectedPlacements.length, actualPlacements.length);
  for (let i = 0; i < minItems; i++) {
    if (typeof expectedPlacements[i] == "string") {
      isPassing = isPassing && actualPlacements[i] == expectedPlacements[i];
    } else if (expectedPlacements[i] instanceof RegExp) {
      isPassing = isPassing && expectedPlacements[i].test(actualPlacements[i]);
    } else {
      ok(false, "Unknown type of expected placement passed to " +
                " assertAreaPlacements. Is your test broken?");
    }
  }
  todo(isPassing, "The area placements for " + areaId +
                  " should equal the expected placements.")
}

function getAreaWidgetIds(areaId) {
  return CustomizableUI.getWidgetIdsInArea(areaId);
}

function simulateItemDrag(toDrag, target) {
  let docId = toDrag.ownerDocument.documentElement.id;
  let dragData = [[{type: 'text/toolbarwrapper-id/' + docId,
                    data: toDrag.id}]];
  synthesizeDragStart(toDrag.parentNode, dragData);
  synthesizeDrop(target, target, dragData);
}

function endCustomizing(aWindow=window) {
  if (aWindow.document.documentElement.getAttribute("customizing") != "true") {
    return true;
  }
  let deferredEndCustomizing = Promise.defer();
  function onCustomizationEnds() {
    aWindow.gNavToolbox.removeEventListener("aftercustomization", onCustomizationEnds);
    deferredEndCustomizing.resolve();
  }
  aWindow.gNavToolbox.addEventListener("aftercustomization", onCustomizationEnds);
  aWindow.gCustomizeMode.exit();

  return deferredEndCustomizing.promise.then(function() {
    let deferredLoadNewTab = Promise.defer();

    //XXXgijs so some tests depend on this tab being about:blank. Make it so.
    let newTabBrowser = aWindow.gBrowser.selectedBrowser;
    newTabBrowser.stop();

    // If we stop early enough, this might actually be about:blank.
    if (newTabBrowser.contentDocument.location.href == "about:blank") {
      return;
    }

    // Otherwise, make it be about:blank, and wait for that to be done.
    function onNewTabLoaded(e) {
      newTabBrowser.removeEventListener("load", onNewTabLoaded, true);
      deferredLoadNewTab.resolve();
    }
    newTabBrowser.addEventListener("load", onNewTabLoaded, true);
    newTabBrowser.contentDocument.location.replace("about:blank");
    return deferredLoadNewTab.promise;
  });
}

function startCustomizing(aWindow=window) {
  if (aWindow.document.documentElement.getAttribute("customizing") == "true") {
    return;
  }
  let deferred = Promise.defer();
  function onCustomizing() {
    aWindow.gNavToolbox.removeEventListener("customizationready", onCustomizing);
    deferred.resolve();
  }
  aWindow.gNavToolbox.addEventListener("customizationready", onCustomizing);
  aWindow.gCustomizeMode.enter();
  return deferred.promise;
}

function openAndLoadWindow(aOptions, aWaitForDelayedStartup=false) {
  let deferred = Promise.defer();
  let win = OpenBrowserWindow(aOptions);
  if (aWaitForDelayedStartup) {
    Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
      if (aSubject != win) {
        return;
      }
      Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
      deferred.resolve(win);
    }, "browser-delayed-startup-finished", false);

  } else {
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad);
      deferred.resolve(win);
    });
  }
  return deferred.promise;
}

function promisePanelShown(win) {
  let panelEl = win.PanelUI.panel;
  let deferred = Promise.defer();
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Panel did not show within 20 seconds.");
  }, 20000);
  function onPanelOpen(e) {
    panelEl.removeEventListener("popupshown", onPanelOpen);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  };
  panelEl.addEventListener("popupshown", onPanelOpen);
  return deferred.promise;
}

function promisePanelHidden(win) {
  let panelEl = win.PanelUI.panel;
  let deferred = Promise.defer();
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Panel did not hide within 20 seconds.");
  }, 20000);
  function onPanelClose(e) {
    panelEl.removeEventListener("popuphidden", onPanelClose);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  }
  panelEl.addEventListener("popuphidden", onPanelClose);
  return deferred.promise;
}

function waitForCondition(aConditionFn, aMaxTries=50, aCheckInterval=100) {
  function tryNow() {
    tries++;
    if (aConditionFn()) {
      deferred.resolve();
    } else if (tries < aMaxTries) {
      tryAgain();
    } else {
      deferred.reject("Condition timed out: " + aConditionFn.toSource());
    }
  }
  function tryAgain() {
    setTimeout(tryNow, aCheckInterval);
  }
  let deferred = Promise.defer();
  let tries = 0;
  tryAgain();
  return deferred.promise;
}

function waitFor(aTimeout=100) {
  let deferred = Promise.defer();
  setTimeout(function() deferred.resolve(), aTimeout);
  return deferred.promise;
}

function testRunner(testAry, asyncCleanup) {
  Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", true);
  for (let test of testAry) {
    info(test.desc);

    if (test.setup)
      yield test.setup();

    info("Running test");
    try {
      yield test.run();
    } catch (ex) {
      ok(false, "Unexpected exception occurred while running the test:\n" + ex);
    }
    info("Cleanup");
    if (test.teardown)
      yield test.teardown();
    ok(!document.getElementById(CustomizableUI.AREA_NAVBAR).hasAttribute("overflowing"), "Shouldn't overflow");
  }
  if (asyncCleanup) {
    yield asyncCleanup();
  }
  ok(CustomizableUI.inDefaultState, "Should remain in default state");
  Services.prefs.clearUserPref("browser.uiCustomization.disableAnimation");
}

function runTests(testAry, asyncCleanup) {
  Task.spawn(testRunner(gTests, asyncCleanup)).then(finish, ex => {
    // The stack of ok() here is misleading due to Promises. The stack of the
    // actual exception is likely much more valuable, hence concatentating it.
    ok(false, "Unexpected exception: " + ex + " With stack: " + ex.stack);
    finish();
  }).then(null, Cu.reportError);
}
