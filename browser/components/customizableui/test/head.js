/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Avoid leaks by using tmp for imports...
var tmp = {};
ChromeUtils.import("resource://gre/modules/Promise.jsm", tmp);
ChromeUtils.import("resource:///modules/CustomizableUI.jsm", tmp);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm", tmp);
ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm",
  tmp
);
var { Promise, CustomizableUI, AppConstants, CustomizableUITestUtils } = tmp;

var EventUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);

/**
 * Instance of CustomizableUITestUtils for the current browser window.
 */
var gCUITestUtils = new CustomizableUITestUtils(window);

Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
registerCleanupFunction(() =>
  Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck")
);

var { synthesizeDrop, synthesizeMouseAtCenter } = EventUtils;

const kForceOverflowWidthPx = 450;

function createDummyXULButton(id, label, win = window) {
  let btn = win.document.createXULElement("toolbarbutton");
  btn.id = id;
  btn.setAttribute("label", label || id);
  btn.className = "toolbarbutton-1 chromeclass-toolbar-additional";
  win.gNavToolbox.palette.appendChild(btn);
  return btn;
}

var gAddedToolbars = new Set();

function createToolbarWithPlacements(id, placements = []) {
  gAddedToolbars.add(id);
  let tb = document.createXULElement("toolbar");
  tb.id = id;
  tb.setAttribute("customizable", "true");
  CustomizableUI.registerArea(id, {
    type: CustomizableUI.TYPE_TOOLBAR,
    defaultPlacements: placements,
  });
  gNavToolbox.appendChild(tb);
  CustomizableUI.registerToolbarNode(tb);
  return tb;
}

function createOverflowableToolbarWithPlacements(id, placements) {
  gAddedToolbars.add(id);

  let tb = document.createXULElement("toolbar");
  tb.id = id;
  tb.setAttribute("customizationtarget", id + "-target");

  let customizationtarget = document.createXULElement("hbox");
  customizationtarget.id = id + "-target";
  customizationtarget.setAttribute("flex", "1");
  tb.appendChild(customizationtarget);

  let overflowPanel = document.createXULElement("panel");
  overflowPanel.id = id + "-overflow";
  document.getElementById("mainPopupSet").appendChild(overflowPanel);

  let overflowList = document.createXULElement("vbox");
  overflowList.id = id + "-overflow-list";
  overflowPanel.appendChild(overflowList);

  let chevron = document.createXULElement("toolbarbutton");
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
  CustomizableUI.registerToolbarNode(tb);
  return tb;
}

function removeCustomToolbars() {
  CustomizableUI.reset();
  for (let toolbarId of gAddedToolbars) {
    CustomizableUI.unregisterArea(toolbarId, true);
    let tb = document.getElementById(toolbarId);
    if (tb.hasAttribute("overflowpanel")) {
      let panel = document.getElementById(tb.getAttribute("overflowpanel"));
      if (panel) {
        panel.remove();
      }
    }
    tb.remove();
  }
  gAddedToolbars.clear();
}

function resetCustomization() {
  return CustomizableUI.reset();
}

function isInDevEdition() {
  return AppConstants.MOZ_DEV_EDITION;
}

function removeNonReleaseButtons(areaPanelPlacements) {
  if (isInDevEdition() && areaPanelPlacements.includes("developer-button")) {
    areaPanelPlacements.splice(
      areaPanelPlacements.indexOf("developer-button"),
      1
    );
  }
}

function removeNonOriginalButtons() {
  CustomizableUI.removeWidgetFromArea("sync-button");
}

function assertAreaPlacements(areaId, expectedPlacements) {
  let actualPlacements = getAreaWidgetIds(areaId);
  placementArraysEqual(areaId, actualPlacements, expectedPlacements);
}

function placementArraysEqual(areaId, actualPlacements, expectedPlacements) {
  info("Actual placements: " + actualPlacements.join(", "));
  info("Expected placements: " + expectedPlacements.join(", "));
  is(
    actualPlacements.length,
    expectedPlacements.length,
    "Area " + areaId + " should have " + expectedPlacements.length + " items."
  );
  let minItems = Math.min(expectedPlacements.length, actualPlacements.length);
  for (let i = 0; i < minItems; i++) {
    if (typeof expectedPlacements[i] == "string") {
      is(
        actualPlacements[i],
        expectedPlacements[i],
        "Item " + i + " in " + areaId + " should match expectations."
      );
    } else if (expectedPlacements[i] instanceof RegExp) {
      ok(
        expectedPlacements[i].test(actualPlacements[i]),
        "Item " +
          i +
          " (" +
          actualPlacements[i] +
          ") in " +
          areaId +
          " should match " +
          expectedPlacements[i]
      );
    } else {
      ok(
        false,
        "Unknown type of expected placement passed to " +
          " assertAreaPlacements. Is your test broken?"
      );
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
      ok(
        false,
        "Unknown type of expected placement passed to " +
          " assertAreaPlacements. Is your test broken?"
      );
    }
  }
  todo(
    isPassing,
    "The area placements for " +
      areaId +
      " should equal the expected placements."
  );
}

function getAreaWidgetIds(areaId) {
  return CustomizableUI.getWidgetIdsInArea(areaId);
}

function simulateItemDrag(aToDrag, aTarget, aEvent = {}, aOffset = 2) {
  let ev = aEvent;
  if (ev == "end" || ev == "start") {
    let win = aTarget.ownerGlobal;
    const dwu = win.windowUtils;
    let bounds = dwu.getBoundsWithoutFlushing(aTarget);
    if (ev == "end") {
      ev = {
        clientX: bounds.right - aOffset,
        clientY: bounds.bottom - aOffset,
      };
    } else {
      ev = { clientX: bounds.left + aOffset, clientY: bounds.top + aOffset };
    }
  }
  ev._domDispatchOnly = true;
  synthesizeDrop(
    aToDrag.parentNode,
    aTarget,
    null,
    null,
    aToDrag.ownerGlobal,
    aTarget.ownerGlobal,
    ev
  );
  // Ensure dnd suppression is cleared.
  synthesizeMouseAtCenter(aTarget, { type: "mouseup" }, aTarget.ownerGlobal);
}

function endCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") != "true") {
    return true;
  }
  let afterCustomizationPromise = BrowserTestUtils.waitForEvent(
    aWindow.gNavToolbox,
    "aftercustomization"
  );
  aWindow.gCustomizeMode.exit();
  return afterCustomizationPromise;
}

function startCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") == "true") {
    return null;
  }
  let customizationReadyPromise = BrowserTestUtils.waitForEvent(
    aWindow.gNavToolbox,
    "customizationready"
  );
  aWindow.gCustomizeMode.enter();
  return customizationReadyPromise;
}

function promiseObserverNotified(aTopic) {
  return new Promise(resolve => {
    Services.obs.addObserver(function onNotification(subject, topic, data) {
      Services.obs.removeObserver(onNotification, topic);
      resolve({ subject, data });
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
      win.addEventListener(
        "load",
        function() {
          resolve(win);
        },
        { once: true }
      );
    }
  });
}

function promiseWindowClosed(win) {
  return new Promise(resolve => {
    win.addEventListener(
      "unload",
      function() {
        resolve();
      },
      { once: true }
    );
    win.close();
  });
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
      executeSoon(resolve);
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
    let obs = new MutationObserver(mutations => {
      for (let mut of mutations) {
        let attr = mut.attributeName;
        let newValue = mut.target.getAttribute(attr);
        if (aFilterFn(newValue)) {
          ok(
            true,
            "mutation occurred: attribute '" +
              attr +
              "' changed to '" +
              newValue +
              "' from '" +
              mut.oldValue +
              "'."
          );
          obs.disconnect();
          resolve();
        } else {
          info(
            "Ignoring mutation that produced value " +
              newValue +
              " because of filter."
          );
        }
      }
    });
    obs.observe(aNode, { attributeFilter: [aAttribute] });
  });
}

function popupShown(aPopup) {
  return BrowserTestUtils.waitForPopupEvent(aPopup, "shown");
}

function popupHidden(aPopup) {
  return BrowserTestUtils.waitForPopupEvent(aPopup, "hidden");
}

// This is a simpler version of the context menu check that
// exists in contextmenu_common.js.
function checkContextMenu(aContextMenu, aExpectedEntries, aWindow = window) {
  let children = [...aContextMenu.children];
  // Ignore hidden nodes:
  children = children.filter(n => !n.hidden);

  for (let i = 0; i < children.length; i++) {
    let menuitem = children[i];
    try {
      if (aExpectedEntries[i][0] == "---") {
        is(menuitem.localName, "menuseparator", "menuseparator expected");
        continue;
      }

      let selector = aExpectedEntries[i][0];
      ok(
        menuitem.matches(selector),
        "menuitem should match " + selector + " selector"
      );
      let commandValue = menuitem.getAttribute("command");
      let relatedCommand = commandValue
        ? aWindow.document.getElementById(commandValue)
        : null;
      let menuItemDisabled = relatedCommand
        ? relatedCommand.getAttribute("disabled") == "true"
        : menuitem.getAttribute("disabled") == "true";
      is(
        menuItemDisabled,
        !aExpectedEntries[i][1],
        "disabled state for " + selector
      );
    } catch (e) {
      ok(false, "Exception when checking context menu: " + e);
    }
  }
}

function waitForOverflowButtonShown(win = window) {
  info("Waiting for overflow button to show");
  let ov = win.document.getElementById("nav-bar-overflow-button");
  return waitForElementShown(ov.icon);
}
function waitForElementShown(element) {
  return BrowserTestUtils.waitForCondition(() => {
    info("Checking if element has non-0 size");
    // We intentionally flush layout to ensure the element is actually shown.
    let rect = element.getBoundingClientRect();
    return rect.width > 0 && rect.height > 0;
  });
}
