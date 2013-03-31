// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var SanitizeHelper = {
  _originalSanitizer: null,

  MockSanitizer: {
    clearCalled: [],
    clearItem: function clearItem(aItemName) {
      info("Clear item called for: " + aItemName);
      this.clearCalled.push(aItemName);
    }
  },

  setUp: function setUp() {
    SanitizeHelper._originalSanitizer = SanitizeUI._sanitizer;
    SanitizeUI._sanitizer = SanitizeHelper.MockSanitizer;
  },

  tearDown: function tearDown() {
    SanitizeUI._sanitizer = SanitizeHelper._originalSanitizer;
  },
};

function getAllSelected() {
  return document.getElementById("prefs-privdata").querySelectorAll(
    "#prefs-privdata-history[checked], " +
      "#prefs-privdata-other[checked] + #prefs-privdata-subitems .privdata-subitem-item[checked]");
}

gTests.push({
  tearDown: SanitizeHelper.tearDown,
  desc: "Test sanitizer UI",
  run: function testSanitizeUI() {
    // Show options flyout
    let promise = waitForEvent(Elements.prefsFlyout, "PopupChanged", 2000);
    Elements.prefsFlyout.show();

    yield promise;

    ok(promise && !(promise instanceof Error), "Wait for PopupChanged");

    // Make sure it's opened
    yield waitForEvent(Elements.prefsFlyout, "transitionend", 1000);

    SanitizeHelper.setUp();

    // Test initial state
    let allSelected = getAllSelected();
    // Only  history should be selected
    ok(allSelected.length === 1 && allSelected[0].getAttribute("itemName") === "history", "History is initially selected.");

    let othersCheck = document.getElementById("prefs-privdata-other");
    othersCheck.setAttribute("checked", "true");

    let othersSubitems = document.getElementById("prefs-privdata-subitems");
    yield waitForCondition(function (){
      return othersSubitems.style.display !== "none";
    }, 500);

    allSelected = getAllSelected();
    // History and all checkboxes under othersSubitems should be selected
    ok(allSelected.length === 1 + othersSubitems.querySelectorAll("checkbox").length,
      "All checkboxes are selected.");

    // Select only downloads and passwords
    let callItems = ["downloads", "passwords"];
    for (let checkbox of allSelected) {
      if (callItems.indexOf(checkbox.getAttribute("itemName")) === -1) {
        checkbox.removeAttribute("checked");
      }
    }

    let clearButton = document.getElementById("prefs-clear-data");
    clearButton.doCommand();

    let clearNotificationDeck = document.getElementById("clear-notification");
    let clearNotificationDone = document.getElementById("clear-notification-done");

    // Wait until command is done.
    yield waitForCondition(function (){
      return clearNotificationDeck.selectedPanel == clearNotificationDone;
    }, 1000);

    ok(SanitizeHelper.MockSanitizer.clearCalled.length === callItems.length, "All expected items were called");

    SanitizeHelper.MockSanitizer.clearCalled.forEach(function(item) {
      ok(callItems.indexOf(item) >= 0, "Sanitized " + item);
    });

    // hide options flyout
    promise = waitForEvent(Elements.prefsFlyout, "PopupChanged", 2000);
    Elements.prefsFlyout.hide();

    yield promise;

    ok(promise && !(promise instanceof Error), "Wait for PopupChanged");
  }
});

function test() {
  runTests();
}
