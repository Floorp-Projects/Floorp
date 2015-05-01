/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * View handler for toolbar events (mostly option toggling and triggering)
 */
let ToolbarView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function *() {
    this._onFilterPopupShowing = this._onFilterPopupShowing.bind(this);
    this._onFilterPopupHiding = this._onFilterPopupHiding.bind(this);
    this._onHiddenMarkersChanged = this._onHiddenMarkersChanged.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);

    this.optionsView = new OptionsView({
      branchName: BRANCH_NAME,
      menupopup: $("#performance-options-menupopup")
    });

    // TODO bug 1160313 get rid of retro mode checks
    // hide option buttons here, and any other buttons in the toolbar
    // (details.js takes care of view buttons)
    if (PerformanceController.getOption("retro-mode")) {
      let RETRO_ELEMENTS = [
        "#option-flatten-tree-recursion", "#option-enable-memory", "#option-invert-flame-graph",
        "#option-show-jit-optimizations", "#filter-button"
      ];
      for (let selector of RETRO_ELEMENTS) {
        $(selector).hidden = true;
      }
    }

    yield this.optionsView.initialize();
    this.optionsView.on("pref-changed", this._onPrefChanged);

    this._buildMarkersFilterPopup();
    this._updateHiddenMarkersPopup();
    $("#performance-filter-menupopup").addEventListener("popupshowing", this._onFilterPopupShowing);
    $("#performance-filter-menupopup").addEventListener("popuphiding",  this._onFilterPopupHiding);
  }),

  /**
   * Unbinds events and cleans up view.
   */
  destroy: function () {
    $("#performance-filter-menupopup").removeEventListener("popupshowing", this._onFilterPopupShowing);
    $("#performance-filter-menupopup").removeEventListener("popuphiding",  this._onFilterPopupHiding);

    this.optionsView.off("pref-changed", this._onPrefChanged);
    this.optionsView.destroy();
  },

  /**
   * Creates the timeline markers filter popup.
   */
  _buildMarkersFilterPopup: function() {
    for (let [markerName, markerDetails] of Iterator(TIMELINE_BLUEPRINT)) {
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("closemenu", "none");
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("align", "center");
      menuitem.setAttribute("flex", "1");
      menuitem.setAttribute("label", markerDetails.label);
      menuitem.setAttribute("marker-type", markerName);
      menuitem.className = markerDetails.colorName;

      menuitem.addEventListener("command", this._onHiddenMarkersChanged);

      $("#performance-filter-menupopup").appendChild(menuitem);
    }
  },

  /**
   * Updates the menu items checked state in the timeline markers filter popup.
   */
  _updateHiddenMarkersPopup: function() {
    let menuItems = $$("#performance-filter-menupopup menuitem[marker-type]");
    let hiddenMarkers = PerformanceController.getPref("hidden-markers");

    for (let menuitem of menuItems) {
      if (~hiddenMarkers.indexOf(menuitem.getAttribute("marker-type"))) {
        menuitem.removeAttribute("checked");
      } else {
        menuitem.setAttribute("checked", "true");
      }
    }
  },

  /**
   * Fired when the markers filter popup starts to show.
   */
  _onFilterPopupShowing: function() {
    $("#filter-button").setAttribute("open", "true");
  },

  /**
   * Fired when the markers filter popup starts to hide.
   */
  _onFilterPopupHiding: function() {
    $("#filter-button").removeAttribute("open");
  },

  /**
   * Fired when a menu item in the markers filter popup is checked or unchecked.
   */
  _onHiddenMarkersChanged: function() {
    let checkedMenuItems = $$("#performance-filter-menupopup menuitem[marker-type]:not([checked])");
    let hiddenMarkers = Array.map(checkedMenuItems, e => e.getAttribute("marker-type"));
    PerformanceController.setPref("hidden-markers", hiddenMarkers);
  },

  /**
   * Fired when a preference changes in the underlying OptionsView.
   * Propogated by the PerformanceController.
   */
  _onPrefChanged: function (_, prefName) {
    let value = Services.prefs.getBoolPref(BRANCH_NAME + prefName);
    this.emit(EVENTS.PREF_CHANGED, prefName, value);
  },

  toString: () => "[object ToolbarView]"
};

EventEmitter.decorate(ToolbarView);
