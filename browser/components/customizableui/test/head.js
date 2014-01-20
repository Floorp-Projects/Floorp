/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Avoid leaks by using tmp for imports...
let tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource:///modules/CustomizableUI.jsm", tmp);
let {Promise, CustomizableUI} = tmp;

let ChromeUtils = {};
let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
registerCleanupFunction(() => Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck"));

let {synthesizeDragStart, synthesizeDrop} = ChromeUtils;

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kTabEventFailureTimeoutInMs = 20000;

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
                  " should equal the expected placements.");
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
  Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", true);
  let deferredEndCustomizing = Promise.defer();
  function onCustomizationEnds() {
    Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", false);
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
  Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", true);
  let deferred = Promise.defer();
  function onCustomizing() {
    aWindow.gNavToolbox.removeEventListener("customizationready", onCustomizing);
    Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", false);
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
  return promisePanelElementShown(win, panelEl);
}

function promiseOverflowShown(win) {
  let panelEl = win.document.getElementById("widget-overflow");
  return promisePanelElementShown(win, panelEl);
}

function promisePanelElementShown(win, aPanel) {
  let deferred = Promise.defer();
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Panel did not show within 20 seconds.");
  }, 20000);
  function onPanelOpen(e) {
    aPanel.removeEventListener("popupshown", onPanelOpen);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  };
  aPanel.addEventListener("popupshown", onPanelOpen);
  return deferred.promise;
}

function promisePanelHidden(win) {
  let panelEl = win.PanelUI.panel;
  return promisePanelElementHidden(win, panelEl);
}

function promiseOverflowHidden(win) {
  let panelEl = document.getElementById("widget-overflow");
  return promisePanelElementHidden(win, panelEl);
}

function promisePanelElementHidden(win, aPanel) {
  let deferred = Promise.defer();
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Panel did not hide within 20 seconds.");
  }, 20000);
  function onPanelClose(e) {
    aPanel.removeEventListener("popuphidden", onPanelClose);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  }
  aPanel.addEventListener("popuphidden", onPanelClose);
  return deferred.promise;
}

function subviewShown(aSubview) {
  let deferred = Promise.defer();
  let win = aSubview.ownerDocument.defaultView;
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Subview (" + aSubview.id + ") did not show within 20 seconds.");
  }, 20000);
  function onViewShowing(e) {
    aSubview.removeEventListener("ViewShowing", onViewShowing);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  };
  aSubview.addEventListener("ViewShowing", onViewShowing);
  return deferred.promise;
}

function subviewHidden(aSubview) {
  let deferred = Promise.defer();
  let win = aSubview.ownerDocument.defaultView;
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Subview (" + aSubview.id + ") did not hide within 20 seconds.");
  }, 20000);
  function onViewHiding(e) {
    aSubview.removeEventListener("ViewHiding", onViewHiding);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  };
  aSubview.addEventListener("ViewHiding", onViewHiding);
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

/**
 * Starts a load in an existing tab and waits for it to finish (via some event).
 *
 * @param aTab       The tab to load into.
 * @param aUrl       The url to load.
 * @param aEventType The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 */
function promiseTabLoadEvent(aTab, aURL, aEventType="load") {
  let deferred = Promise.defer();
  info("Wait for tab event: " + aEventType);

  let timeoutId = setTimeout(() => {
    aTab.linkedBrowser.removeEventListener(aEventType, onTabLoad, true);
    deferred.reject("TabSelect did not happen within " + kTabEventFailureTimeoutInMs + "ms");
  }, kTabEventFailureTimeoutInMs);

  function onTabLoad(event) {
    if (event.originalTarget != aTab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      info("skipping spurious load event");
      return;
    }
    clearTimeout(timeoutId);
    aTab.linkedBrowser.removeEventListener(aEventType, onTabLoad, true);
    info("Tab event received: " + aEventType);
    deferred.resolve();
  }
  aTab.linkedBrowser.addEventListener(aEventType, onTabLoad, true, true);
  aTab.linkedBrowser.loadURI(aURL);
  return deferred.promise;
}

/**
 * Navigate back or forward in tab history and wait for it to finish.
 *
 * @param aDirection   Number to indicate to move backward or forward in history.
 * @param aConditionFn Function that returns the result of an evaluated condition
 *                     that needs to be `true` to resolve the promise.
 * @return {Promise} resolved when navigation has finished.
 */
function promiseTabHistoryNavigation(aDirection = -1, aConditionFn) {
  let deferred = Promise.defer();

  let timeoutId = setTimeout(() => {
    gBrowser.removeEventListener("pageshow", listener, true);
    deferred.reject("Pageshow did not happen within " + kTabEventFailureTimeoutInMs + "ms");
  }, kTabEventFailureTimeoutInMs);

  function listener(event) {
    gBrowser.removeEventListener("pageshow", listener, true);
    clearTimeout(timeoutId);

    if (aConditionFn) {
      waitForCondition(aConditionFn).then(() => deferred.resolve(),
                                          aReason => deferred.reject(aReason));
    } else {
      deferred.resolve();
    }
  }
  gBrowser.addEventListener("pageshow", listener, true);

  content.history.go(aDirection);

  return deferred.promise;
}
