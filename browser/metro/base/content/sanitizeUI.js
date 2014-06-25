// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var SanitizeUI = {
  _sanitizer: null,

  _privDataElement: null,
  get _privData() {
    if (this._privDataElement === null) {
      this._privDataElement = document.getElementById("prefs-privdata");
    }
    return this._privDataElement;
  },

  init: function () {
    this._sanitizer = new Sanitizer();
    this._privData.addEventListener("CheckboxStateChange", this, true);
  },

  _clearNotificationTimeout: null,
  onSanitize: function onSanitize() {
    let button = document.getElementById("prefs-clear-data");
    let clearNotificationDeck = document.getElementById("clear-notification");
    let clearNotificationEmpty = document.getElementById("clear-notification-empty");
    let clearNotificationClearing = document.getElementById("clear-notification-clearing");
    let clearNotificationDone = document.getElementById("clear-notification-done");
    let allCheckboxes = SanitizeUI._privData.querySelectorAll("checkbox");
    let allSelected = SanitizeUI._privData.querySelectorAll(
      "#prefs-privdata-history[checked], " +
      "#prefs-privdata-other[checked] + #prefs-privdata-subitems .privdata-subitem-item[checked]");

    // disable button and checkboxes temporarily to indicate something is happening
    button.disabled = true;
    for (let checkbox of allCheckboxes) {
      checkbox.disabled = true;
    }
    clearNotificationDeck.selectedPanel = clearNotificationClearing;
    document.getElementById("clearprivacythrobber").enabled = true;

    // Run asynchronously to let UI update
    setTimeout(function() {
      for (let item of allSelected) {
        let itemName = item.getAttribute("itemName");

        try {
          SanitizeUI._sanitizer.clearItem(itemName);
        } catch(e) {
          Components.utils.reportError("Error sanitizing " + itemName + ": " + e);
        }
      }

      button.disabled = false;
      for (let checkbox of allCheckboxes) {
        checkbox.disabled = false;
      }
      clearNotificationDeck.selectedPanel = clearNotificationDone;
      document.getElementById("clearprivacythrobber").enabled = false;

      // Clear notifications after 4 seconds
      clearTimeout(SanitizeUI._clearNotificationTimeout);
      SanitizeUI._clearNotificationTimeout = setTimeout(function() {
        clearNotificationDeck.selectedPanel = clearNotificationEmpty;
      }, 4000);
    }, 0);
  },

  /* Disable the clear button when nothing is selected */
  _onCheckboxChange: function _onCheckboxChange() {
    let anySelected = SanitizeUI._privData.querySelector(
      "#prefs-privdata-history[checked], " +
      "#prefs-privdata-other[checked] + #prefs-privdata-subitems .privdata-subitem-item[checked]");

    let clearButton = document.getElementById("prefs-clear-data");
    clearButton.disabled = !anySelected;
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "CheckboxStateChange":
        this._onCheckboxChange();
        break;
    }
  },
};