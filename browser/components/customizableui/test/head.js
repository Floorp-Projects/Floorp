/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Avoid leaks by using tmp for imports...
var tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource:///modules/CustomizableUI.jsm", tmp);
var {Promise, CustomizableUI} = tmp;

var ChromeUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
registerCleanupFunction(() => Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck"));

// Remove temporary e10s related new window options in customize ui,
// they break a lot of tests.
CustomizableUI.destroyWidget("e10s-button");
CustomizableUI.removeWidgetFromArea("e10s-button");

var {synthesizeDragStart, synthesizeDrop} = ChromeUtils;

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kTabEventFailureTimeoutInMs = 20000;

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

XPCOMUtils.defineLazyGetter(this, 'gDeveloperButtonInNavbar', function() {
  return getAreaWidgetIds(CustomizableUI.AREA_NAVBAR).indexOf("developer-button") != -1;
});

function isInDevEdition() {
  return gDeveloperButtonInNavbar;
}

function removeDeveloperButtonIfDevEdition(areaPanelPlacements) {
  if (isInDevEdition()) {
    areaPanelPlacements.splice(areaPanelPlacements.indexOf("developer-button"), 1);
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

function simulateItemDrag(aToDrag, aTarget) {
  synthesizeDrop(aToDrag.parentNode, aTarget);
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
    if (newTabBrowser.currentURI.spec == "about:blank") {
      return null;
    }

    // Otherwise, make it be about:blank, and wait for that to be done.
    function onNewTabLoaded(e) {
      newTabBrowser.removeEventListener("load", onNewTabLoaded, true);
      deferredLoadNewTab.resolve();
    }
    newTabBrowser.addEventListener("load", onNewTabLoaded, true);
    newTabBrowser.loadURI("about:blank");
    return deferredLoadNewTab.promise;
  });
}

function startCustomizing(aWindow=window) {
  if (aWindow.document.documentElement.getAttribute("customizing") == "true") {
    return null;
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

function promiseObserverNotified(aTopic) {
  let deferred = Promise.defer();
  Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
    Services.obs.removeObserver(onNotification, aTopic);
      deferred.resolve({subject: aSubject, data: aData});
    }, aTopic, false);
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

function promiseWindowClosed(win) {
  let deferred = Promise.defer();
  win.addEventListener("unload", function onunload() {
    win.removeEventListener("unload", onunload);
    deferred.resolve();
  });
  win.close();
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
  }
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

function isPanelUIOpen() {
  return PanelUI.panel.state == "open" || PanelUI.panel.state == "showing";
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
  }
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
  }
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
  setTimeout(() => deferred.resolve(), aTimeout);
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
  let deferred = Promise.defer();
  let win = aPopup.ownerDocument.defaultView;
  let eventType = "popup" + aEventSuffix;

  function onPopupEvent(e) {
    aPopup.removeEventListener(eventType, onPopupEvent);
    deferred.resolve();
  }

  aPopup.addEventListener(eventType, onPopupEvent);
  return deferred.promise;
}

// This is a simpler version of the context menu check that
// exists in contextmenu_common.js.
function checkContextMenu(aContextMenu, aExpectedEntries, aWindow=window) {
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
