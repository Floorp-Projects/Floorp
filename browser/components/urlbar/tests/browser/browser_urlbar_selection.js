/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const exampleSearch = "f oo  bar";
const exampleUrl = "https://example.com/1";

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

function click(target) {
  let promise = BrowserTestUtils.waitForEvent(target, "click");
  EventUtils.synthesizeMouseAtCenter(target, {}, target.ownerGlobal);
  return promise;
}

function openContextMenu(target) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    target.ownerGlobal,
    "contextmenu"
  );

  EventUtils.synthesizeMouseAtCenter(
    target,
    {
      type: "contextmenu",
      button: 2,
    },
    target.ownerGlobal
  );
  return popupShownPromise;
}

function drag(target, fromX, fromY, toX, toY) {
  let promise = BrowserTestUtils.waitForEvent(target, "mouseup");
  EventUtils.synthesizeMouse(
    target,
    fromX,
    fromY,
    { type: "mousemove" },
    target.ownerGlobal
  );
  EventUtils.synthesizeMouse(
    target,
    fromX,
    fromY,
    { type: "mousedown" },
    target.ownerGlobal
  );
  EventUtils.synthesizeMouse(
    target,
    toX,
    toY,
    { type: "mousemove" },
    target.ownerGlobal
  );
  EventUtils.synthesizeMouse(
    target,
    toX,
    toY,
    { type: "mouseup" },
    target.ownerGlobal
  );
  return promise;
}

add_task(async function setup() {
  SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.clickSelectsAll", true]],
  });

  // On macOS, we must "warm up" the Urlbar to get the first test to pass.
  gURLBar.value = "";
  await click(gURLBar.inputField);
  gURLBar.blur();
});

add_task(async function leftClickSelectsAll() {
  gURLBar.value = exampleSearch;
  await click(gURLBar.inputField);
  Assert.equal(
    gURLBar.selectionStart,
    0,
    "The entire search term should be selected."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    exampleSearch.length,
    "The entire search term should be selected."
  );
  gURLBar.blur();
});

add_task(async function leftClickSelectsUrl() {
  gURLBar.value = exampleUrl;
  await click(gURLBar.inputField);
  Assert.equal(gURLBar.selectionStart, 0, "The entire url should be selected.");
  Assert.equal(
    gURLBar.selectionEnd,
    exampleUrl.length,
    "The entire url should be selected."
  );
  gURLBar.blur();
});

// Test to ensure that the doubleClickSelectsAll pref does not interfere with
// single click behaviour (Double CSA itself is tested in
// urlbar/tests/browser_doubleClickSelectsAll.js).
add_task(async function bothPrefsEnabled() {
  Services.prefs.setBoolPref("browser.urlbar.doubleClickSelectsAll", true);
  gURLBar.value = exampleSearch;
  await click(gURLBar.inputField);
  Assert.equal(
    gURLBar.selectionStart,
    0,
    "The entire search term should be selected."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    exampleSearch.length,
    "The entire search term should be selected."
  );
  gURLBar.blur();
  Services.prefs.clearUserPref("browser.urlbar.doubleClickSelectsAll");
});

add_task(async function rightClickSelectsAll() {
  // The text should be selected even when the pref is disabled.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.clickSelectsAll", false]],
  });

  gURLBar.inputField.focus();
  gURLBar.value = exampleUrl;

  // Remove the selection so the focus() call above doesn't influence the test.
  gURLBar.selectionStart = gURLBar.selectionEnd = 0;

  await openContextMenu(gURLBar.inputField);

  Assert.equal(gURLBar.selectionStart, 0, "The entire URL should be selected.");
  Assert.equal(
    gURLBar.selectionEnd,
    exampleUrl.length,
    "The entire URL should be selected."
  );

  let contextMenu = gURLBar.querySelector("moz-input-box").menupopup;

  // While the context menu is open, test the "Select All" button.
  let contextMenuItem = contextMenu.firstElementChild;
  while (
    contextMenuItem.nextElementSibling &&
    contextMenuItem.getAttribute("cmd") != "cmd_selectAll"
  ) {
    contextMenuItem = contextMenuItem.nextElementSibling;
  }
  Assert.ok(
    contextMenuItem,
    "The context menu should have the select all menu item."
  );

  let controller = document.commandDispatcher.getControllerForCommand(
    contextMenuItem.getAttribute("cmd")
  );
  let enabled = controller.isCommandEnabled(
    contextMenuItem.getAttribute("cmd")
  );
  Assert.ok(enabled, "The context menu select all item should be enabled.");

  await click(contextMenuItem);
  Assert.equal(
    gURLBar.selectionStart,
    0,
    "The entire URL should be selected after clicking selectAll button."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    exampleUrl.length,
    "The entire URL should be selected after clicking selectAll button."
  );

  gURLBar.querySelector("moz-input-box").menupopup.hidePopup();
  gURLBar.blur();
  await SpecialPowers.popPrefEnv();
});

add_task(async function contextMenuDoesNotCancelSelection() {
  gURLBar.inputField.focus();
  gURLBar.value = exampleUrl;

  gURLBar.selectionStart = 3;
  gURLBar.selectionEnd = 7;

  await openContextMenu(gURLBar.inputField);

  Assert.equal(
    gURLBar.selectionStart,
    3,
    "The selection should not have changed."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    7,
    "The selection should not have changed."
  );

  gURLBar.querySelector("moz-input-box").menupopup.hidePopup();
  gURLBar.blur();
});

add_task(async function dragSelect() {
  gURLBar.value = exampleSearch.repeat(10);
  // Drags from an artibrary offset of 30 to test for bug 1562145: that the
  // selection does not start at the beginning.
  await drag(gURLBar.inputField, 30, 0, 60, 0);
  Assert.greater(
    gURLBar.selectionStart,
    0,
    "Selection should not start at the beginning of the string."
  );

  gURLBar.blur();
});

/**
 * Testing for bug 1571018: that the entire Urlbar isn't selected when the
 * Urlbar is dragged following a selectsAll event then a blur.
 */
add_task(async function dragAfterSelectAll() {
  gURLBar.value = exampleSearch.repeat(10);
  await click(gURLBar.inputField);
  Assert.equal(
    gURLBar.selectionStart,
    0,
    "The entire search term should be selected."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    exampleSearch.repeat(10).length,
    "The entire search term should be selected."
  );

  gURLBar.blur();

  // The offset of 30 is arbitrary.
  await drag(gURLBar.inputField, 30, 0, 60, 0);

  Assert.notEqual(
    gURLBar.selectionStart,
    0,
    "Only part of the search term should be selected."
  );
  Assert.notEqual(
    gURLBar.selectionEnd,
    exampleSearch.repeat(10).length,
    "Only part of the search term should be selected."
  );
});

/**
 * Testing for bug 1571018: that the entire Urlbar is selected when the Urlbar
 * is refocused following a partial text selection then a blur.
 */
add_task(async function selectAllAfterDrag() {
  gURLBar.value = exampleSearch;

  gURLBar.selectionStart = 3;
  gURLBar.selectionEnd = 7;

  gURLBar.blur();

  await click(gURLBar.inputField);

  Assert.equal(
    gURLBar.selectionStart,
    0,
    "The entire search term should be selected."
  );
  Assert.equal(
    gURLBar.selectionEnd,
    exampleSearch.length,
    "The entire search term should be selected."
  );

  gURLBar.blur();
});
