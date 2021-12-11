/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, $$, PerformanceController */
"use strict";

const EVENTS = require("devtools/client/performance/events");
const {
  TIMELINE_BLUEPRINT,
} = require("devtools/client/performance/modules/markers");
const {
  MarkerBlueprintUtils,
} = require("devtools/client/performance/modules/marker-blueprint-utils");

const { OptionsView } = require("devtools/client/shared/options-view");
const EventEmitter = require("devtools/shared/event-emitter");

const BRANCH_NAME = "devtools.performance.ui.";

/**
 * View handler for toolbar events (mostly option toggling and triggering)
 */
const ToolbarView = {
  /**
   * Sets up the view with event binding.
   */
  async initialize() {
    this._onFilterPopupShowing = this._onFilterPopupShowing.bind(this);
    this._onFilterPopupHiding = this._onFilterPopupHiding.bind(this);
    this._onHiddenMarkersChanged = this._onHiddenMarkersChanged.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._popup = $("#performance-options-menupopup");

    this.optionsView = new OptionsView({
      branchName: BRANCH_NAME,
      menupopup: this._popup,
    });

    // Set the visibility of experimental UI options on load
    // based off of `devtools.performance.ui.experimental` preference
    const experimentalEnabled = PerformanceController.getOption("experimental");
    this._toggleExperimentalUI(experimentalEnabled);

    await this.optionsView.initialize();
    this.optionsView.on("pref-changed", this._onPrefChanged);

    this._buildMarkersFilterPopup();
    this._updateHiddenMarkersPopup();
    $("#performance-filter-menupopup").addEventListener(
      "popupshowing",
      this._onFilterPopupShowing
    );
    $("#performance-filter-menupopup").addEventListener(
      "popuphiding",
      this._onFilterPopupHiding
    );
  },

  /**
   * Unbinds events and cleans up view.
   */
  destroy: function() {
    $("#performance-filter-menupopup").removeEventListener(
      "popupshowing",
      this._onFilterPopupShowing
    );
    $("#performance-filter-menupopup").removeEventListener(
      "popuphiding",
      this._onFilterPopupHiding
    );
    this._popup = null;

    this.optionsView.off("pref-changed", this._onPrefChanged);
    this.optionsView.destroy();
  },

  /**
   * Creates the timeline markers filter popup.
   */
  _buildMarkersFilterPopup: function() {
    for (const [markerName, markerDetails] of Object.entries(
      TIMELINE_BLUEPRINT
    )) {
      const menuitem = document.createXULElement("menuitem");
      menuitem.setAttribute("closemenu", "none");
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("align", "center");
      menuitem.setAttribute("flex", "1");
      menuitem.setAttribute(
        "label",
        MarkerBlueprintUtils.getMarkerGenericName(markerName)
      );
      menuitem.setAttribute("marker-type", markerName);
      menuitem.className = `marker-color-${markerDetails.colorName}`;

      menuitem.addEventListener("command", this._onHiddenMarkersChanged);

      $("#performance-filter-menupopup").appendChild(menuitem);
    }
  },

  /**
   * Updates the menu items checked state in the timeline markers filter popup.
   */
  _updateHiddenMarkersPopup: function() {
    const menuItems = $$("#performance-filter-menupopup menuitem[marker-type]");
    const hiddenMarkers = PerformanceController.getPref("hidden-markers");

    for (const menuitem of menuItems) {
      if (~hiddenMarkers.indexOf(menuitem.getAttribute("marker-type"))) {
        menuitem.removeAttribute("checked");
      } else {
        menuitem.setAttribute("checked", "true");
      }
    }
  },

  /**
   * Fired when `devtools.performance.ui.experimental` is changed, or
   * during init. Toggles the visibility of experimental performance tool options
   * in the UI options.
   *
   * Sets or removes "experimental-enabled" on the menu and main elements,
   * hiding or showing all elements with class "experimental-option".
   *
   * TODO re-enable "#option-enable-memory" permanently once stable in bug 1163350
   * TODO re-enable "#option-show-jit-optimizations" permanently once stable in
   *      bug 1163351
   *
   * @param {boolean} isEnabled
   */
  _toggleExperimentalUI: function(isEnabled) {
    if (isEnabled) {
      $(".theme-body").classList.add("experimental-enabled");
      this._popup.classList.add("experimental-enabled");
    } else {
      $(".theme-body").classList.remove("experimental-enabled");
      this._popup.classList.remove("experimental-enabled");
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
    const checkedMenuItems = $$(
      "#performance-filter-menupopup menuitem[marker-type]:not([checked])"
    );
    const hiddenMarkers = Array.from(checkedMenuItems, e =>
      e.getAttribute("marker-type")
    );
    PerformanceController.setPref("hidden-markers", hiddenMarkers);
  },

  /**
   * Fired when a preference changes in the underlying OptionsView.
   * Propogated by the PerformanceController.
   */
  _onPrefChanged: function(prefName) {
    const value = PerformanceController.getOption(prefName);

    if (prefName === "experimental") {
      this._toggleExperimentalUI(value);
    }

    this.emit(EVENTS.UI_PREF_CHANGED, prefName, value);
  },

  toString: () => "[object ToolbarView]",
};

EventEmitter.decorate(ToolbarView);

exports.ToolbarView = window.ToolbarView = ToolbarView;
