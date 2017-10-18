/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Avoid leaks by using tmp for imports...
var tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource:///modules/CustomizableUI.jsm", tmp);
Cu.import("resource://gre/modules/AppConstants.jsm", tmp);
var {Promise, CustomizableUI, AppConstants} = tmp;

var EventUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
registerCleanupFunction(() => Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck"));

var {synthesizeDragStart, synthesizeDrop} = EventUtils;

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kTabEventFailureTimeoutInMs = 20000;

const kForceOverflowWidthPx = 300;

function createDummyXULButton(id, label, win = window) {
  let btn = document.createElementNS(kNSXUL, "toolbarbutton");
  btn.id = id;
  btn.setAttribute("label", label || id);
  btn.className = "toolbarbutton-1 chromeclass-toolbar-additional";
  win.gNavToolbox.palette.appendChild(btn);
  return btn;
}

var gAddedToolbars = new Set();

function createToolbarWithPlacements(id, placements = []) {
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

function createOverflowableToolbarWithPlacements(id, placements) {
  gAddedToolbars.add(id);

  let tb = document.createElementNS(kNSXUL, "toolbar");
  tb.id = id;
  tb.setAttribute("customizationtarget", id + "-target");

  let customizationtarget = document.createElementNS(kNSXUL, "hbox");
  customizationtarget.id = id + "-target";
  customizationtarget.setAttribute("flex", "1");
  tb.appendChild(customizationtarget);

  let overflowPanel = document.createElementNS(kNSXUL, "panel");
  overflowPanel.id = id + "-overflow";
  document.getElementById("mainPopupSet").appendChild(overflowPanel);

  let overflowList = document.createElementNS(kNSXUL, "vbox");
  overflowList.id = id + "-overflow-list";
  overflowPanel.appendChild(overflowList);

  let chevron = document.createElementNS(kNSXUL, "toolbarbutton");
  chevron.id = id + "-chevron";
  tb.appendChild(chevron);

  CustomizableUI.registerArea(id, {
    type: CustomizableUI.TYPE_TOOLBAR,
    defaultPlacements: placements,
    overflowable: true,
  });

  tb.setAttribute("customizable", "true");
  tb.setAttribute("overflowable", "true");
  tb.setAttribute("overflowpanel", overflowPanel.id);
  tb.setAttribute("overflowtarget", overflowList.id);
  tb.setAttribute("overflowbutton", chevron.id);

  gNavToolbox.appendChild(tb);
  return tb;
}

function removeCustomToolbars() {
  CustomizableUI.reset();
  for (let toolbarId of gAddedToolbars) {
    CustomizableUI.unregisterArea(toolbarId, true);
    let tb = document.getElementById(toolbarId);
    if (tb.hasAttribute("overflowpanel")) {
      let panel = document.getElementById(tb.getAttribute("overflowpanel"));
      if (panel)
        panel.remove();
    }
    tb.remove();
  }
  gAddedToolbars.clear();
}

function getToolboxCustomToolbarId(toolbarName) {
  return "__customToolbar_" + toolbarName.replace(" ", "_");
}

function resetCustomization() {
  return CustomizableUI.reset();
}

function isInDevEdition() {
  return AppConstants.MOZ_DEV_EDITION;
}

function removeNonReleaseButtons(areaPanelPlacements) {
  if (isInDevEdition() && areaPanelPlacements.includes("developer-button")) {
    areaPanelPlacements.splice(areaPanelPlacements.indexOf("developer-button"), 1);
  }
}

function removeNonOriginalButtons() {
  CustomizableUI.removeWidgetFromArea("sync-button");
}

function restoreNonOriginalButtons() {
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
}

function assertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  placementArraysEqual(areaId, actualPlacements, expectedPlacements);
}

function placementArraysEqual(areaId, actualPlacements, expectedPlacements) {
  info("Actual placements: " + actualPlacements.join(", "));
  info("Expected placements: " + expectedPlacements.join(", "));
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

function simulateItemDrag(aToDrag, aTarget) {
  synthesizeDrop(aToDrag.parentNode, aTarget);
}

function endCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") != "true") {
    return true;
  }
  return new Promise(resolve => {
    function onCustomizationEnds() {
      aWindow.gNavToolbox.removeEventListener("aftercustomization", onCustomizationEnds);
      resolve();
    }
    aWindow.gNavToolbox.addEventListener("aftercustomization", onCustomizationEnds);
    aWindow.gCustomizeMode.exit();

  });
}

function startCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") == "true") {
    return null;
  }
  return new Promise(resolve => {
    function onCustomizing() {
      aWindow.gNavToolbox.removeEventListener("customizationready", onCustomizing);
      resolve();
    }
    aWindow.gNavToolbox.addEventListener("customizationready", onCustomizing);
    aWindow.gCustomizeMode.enter();
  });
}

function promiseObserverNotified(aTopic) {
  return new Promise(resolve => {
    Services.obs.addObserver(function onNotification(subject, topic, data) {
      Services.obs.removeObserver(onNotification, topic);
        resolve({subject, data});
      }, aTopic);
  });
}

function openAndLoadWindow(aOptions, aWaitForDelayedStartup = false) {
  return new Promise(resolve => {
    let win = OpenBrowserWindow(aOptions);
    if (aWaitForDelayedStartup) {
      Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
        if (aSubject != win) {
          return;
        }
        Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
        resolve(win);
      }, "browser-delayed-startup-finished");

    } else {
      win.addEventListener("load", function() {
        resolve(win);
      }, {once: true});
    }
  });
}

function promiseWindowClosed(win) {
  return new Promise(resolve => {
    win.addEventListener("unload", function() {
      resolve();
    }, {once: true});
    win.close();
  });
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
  return new Promise((resolve, reject) => {
    let timeoutId = win.setTimeout(() => {
      reject("Panel did not show within 20 seconds.");
    }, 20000);
    function onPanelOpen(e) {
      aPanel.removeEventListener("popupshown", onPanelOpen);
      win.clearTimeout(timeoutId);
      resolve();
    }
    aPanel.addEventListener("popupshown", onPanelOpen);
  });
}

function promisePanelHidden(win) {
  let panelEl = win.PanelUI.panel;
  return promisePanelElementHidden(win, panelEl);
}

function promiseOverflowHidden(win) {
  let panelEl = win.PanelUI.overflowPanel;
  return promisePanelElementHidden(win, panelEl);
}

function promisePanelElementHidden(win, aPanel) {
  return new Promise((resolve, reject) => {
    let timeoutId = win.setTimeout(() => {
      reject("Panel did not hide within 20 seconds.");
    }, 20000);
    function onPanelClose(e) {
      aPanel.removeEventListener("popuphidden", onPanelClose);
      win.clearTimeout(timeoutId);
      resolve();
    }
    aPanel.addEventListener("popuphidden", onPanelClose);
  });
}

function isPanelUIOpen() {
  return PanelUI.panel.state == "open" || PanelUI.panel.state == "showing";
}

function isOverflowOpen() {
  let panel = document.getElementById("widget-overflow");
  return panel.state == "open" || panel.state == "showing";
}

function subviewShown(aSubview) {
  return new Promise((resolve, reject) => {
    let win = aSubview.ownerGlobal;
    let timeoutId = win.setTimeout(() => {
      reject("Subview (" + aSubview.id + ") did not show within 20 seconds.");
    }, 20000);
    function onViewShown(e) {
      aSubview.removeEventListener("ViewShown", onViewShown);
      win.clearTimeout(timeoutId);
      resolve();
    }
    aSubview.addEventListener("ViewShown", onViewShown);
  });
}

function subviewHidden(aSubview) {
  return new Promise((resolve, reject) => {
    let win = aSubview.ownerGlobal;
    let timeoutId = win.setTimeout(() => {
      reject("Subview (" + aSubview.id + ") did not hide within 20 seconds.");
    }, 20000);
    function onViewHiding(e) {
      aSubview.removeEventListener("ViewHiding", onViewHiding);
      win.clearTimeout(timeoutId);
      resolve();
    }
    aSubview.addEventListener("ViewHiding", onViewHiding);
  });
}

function waitForCondition(aConditionFn, aMaxTries = 50, aCheckInterval = 100) {
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

function waitFor(aTimeout = 100) {
  return new Promise(resolve => {
    setTimeout(() => resolve(), aTimeout);
  });
}

/**
 * Starts a load in an existing tab and waits for it to finish (via some event).
 *
 * @param aTab       The tab to load into.
 * @param aUrl       The url to load.
 * @param aEventType The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 */
function promiseTabLoadEvent(aTab, aURL) {
  let browser = aTab.linkedBrowser;

  BrowserTestUtils.loadURI(browser, aURL);
  return BrowserTestUtils.browserLoaded(browser);
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
  return new Promise((resolve, reject) => {

    let timeoutId = setTimeout(() => {
      gBrowser.removeEventListener("pageshow", listener, true);
      reject("Pageshow did not happen within " + kTabEventFailureTimeoutInMs + "ms");
    }, kTabEventFailureTimeoutInMs);

    function listener(event) {
      gBrowser.removeEventListener("pageshow", listener, true);
      clearTimeout(timeoutId);

      if (aConditionFn) {
        waitForCondition(aConditionFn).then(() => resolve(),
                                            aReason => reject(aReason));
      } else {
        resolve();
      }
    }
    gBrowser.addEventListener("pageshow", listener, true);

    content.history.go(aDirection);

  });
}

/**
 * Wait for an attribute on a node to change
 *
 * @param aNode      Node on which the mutation is expected
 * @param aAttribute The attribute we're interested in
 * @param aFilterFn  A function to check if the new value is what we want.
 * @return {Promise} resolved when the requisite mutation shows up.
 */
function promiseAttributeMutation(aNode, aAttribute, aFilterFn) {
  return new Promise((resolve, reject) => {
    info("waiting for mutation of attribute '" + aAttribute + "'.");
    let obs = new MutationObserver((mutations) => {
      for (let mut of mutations) {
        let attr = mut.attributeName;
        let newValue = mut.target.getAttribute(attr);
        if (aFilterFn(newValue)) {
          ok(true, "mutation occurred: attribute '" + attr + "' changed to '" + newValue + "' from '" + mut.oldValue + "'.");
          obs.disconnect();
          resolve();
        } else {
          info("Ignoring mutation that produced value " + newValue + " because of filter.");
        }
      }
    });
    obs.observe(aNode, {attributeFilter: [aAttribute]});
  });
}

function popupShown(aPopup) {
  return promisePopupEvent(aPopup, "shown");
}

function popupHidden(aPopup) {
  return promisePopupEvent(aPopup, "hidden");
}

/**
 * Returns a Promise that resolves when aPopup fires an event of type
 * aEventType. Times out and rejects after 20 seconds.
 *
 * @param aPopup the popup to monitor for events.
 * @param aEventSuffix the _suffix_ for the popup event type to watch for.
 *
 * Example usage:
 *   let popupShownPromise = promisePopupEvent(somePopup, "shown");
 *   // ... something that opens a popup
 *   yield popupShownPromise;
 *
 *  let popupHiddenPromise = promisePopupEvent(somePopup, "hidden");
 *  // ... something that hides a popup
 *  yield popupHiddenPromise;
 */
function promisePopupEvent(aPopup, aEventSuffix) {
  return new Promise(resolve => {
    let eventType = "popup" + aEventSuffix;

    function onPopupEvent(e) {
      aPopup.removeEventListener(eventType, onPopupEvent);
      resolve();
    }

    aPopup.addEventListener(eventType, onPopupEvent);
  });
}

// This is a simpler version of the context menu check that
// exists in contextmenu_common.js.
function checkContextMenu(aContextMenu, aExpectedEntries, aWindow = window) {
  let childNodes = [...aContextMenu.childNodes];
  // Ignore hidden nodes:
  childNodes = childNodes.filter((n) => !n.hidden);

  for (let i = 0; i < childNodes.length; i++) {
    let menuitem = childNodes[i];
    try {
      if (aExpectedEntries[i][0] == "---") {
        is(menuitem.localName, "menuseparator", "menuseparator expected");
        continue;
      }

      let selector = aExpectedEntries[i][0];
      ok(menuitem.matches(selector), "menuitem should match " + selector + " selector");
      let commandValue = menuitem.getAttribute("command");
      let relatedCommand = commandValue ? aWindow.document.getElementById(commandValue) : null;
      let menuItemDisabled = relatedCommand ?
                               relatedCommand.getAttribute("disabled") == "true" :
                               menuitem.getAttribute("disabled") == "true";
      is(menuItemDisabled, !aExpectedEntries[i][1], "disabled state for " + selector);
    } catch (e) {
      ok(false, "Exception when checking context menu: " + e);
    }
  }
}

function waitForOverflowButtonShown(win = window) {
  let dwu = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
  return BrowserTestUtils.waitForCondition(() => {
    info("Waiting for overflow button to have non-0 size");
    let ov = win.document.getElementById("nav-bar-overflow-button");
    let icon = win.document.getAnonymousElementByAttribute(ov, "class", "toolbarbutton-icon");
    let bounds = dwu.getBoundsWithoutFlushing(icon);
    return bounds.width > 0 && bounds.height > 0;
  });
}
