/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Keeps the "browser.search.widget.inNavBar" preference synchronized,
 * and ensures persisted widths are updated if the search bar is removed.
 */

"use strict";

var EXPORTED_SYMBOLS = ["SearchWidgetTracker"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "BrowserUsageTelemetry",
  "resource:///modules/BrowserUsageTelemetry.jsm"
);

const WIDGET_ID = "search-container";
const PREF_NAME = "browser.search.widget.inNavBar";

const SearchWidgetTracker = {
  init() {
    this.onWidgetReset = this.onWidgetUndoMove = node => {
      if (node.id == WIDGET_ID) {
        this.syncPreferenceWithWidget();
        this.removePersistedWidths();
      }
    };
    lazy.CustomizableUI.addListener(this);
    Services.prefs.addObserver(PREF_NAME, () =>
      this.syncWidgetWithPreference()
    );
  },

  onWidgetAdded(widgetId, area) {
    if (widgetId == WIDGET_ID && area == lazy.CustomizableUI.AREA_NAVBAR) {
      this.syncPreferenceWithWidget();
    }
  },

  onWidgetRemoved(aWidgetId, aArea) {
    if (aWidgetId == WIDGET_ID && aArea == lazy.CustomizableUI.AREA_NAVBAR) {
      this.syncPreferenceWithWidget();
      this.removePersistedWidths();
    }
  },

  onAreaNodeRegistered(aArea) {
    // The placement of the widget always takes priority, and the preference
    // should always match the actual placement when the browser starts up - i.e.
    // once the navigation bar has been registered.
    if (aArea == lazy.CustomizableUI.AREA_NAVBAR) {
      this.syncPreferenceWithWidget();
    }
  },

  onCustomizeEnd() {
    // onWidgetUndoMove does not fire when the search container is moved back to
    // the customization palette as a result of an undo, so we sync again here.
    this.syncPreferenceWithWidget();
  },

  syncPreferenceWithWidget() {
    Services.prefs.setBoolPref(PREF_NAME, this.widgetIsInNavBar);
  },

  syncWidgetWithPreference() {
    let newValue = Services.prefs.getBoolPref(PREF_NAME);
    if (newValue == this.widgetIsInNavBar) {
      return;
    }

    if (newValue) {
      // The URL bar widget is always present in the navigation toolbar, so we
      // can simply read its position to place the search bar right after it.
      lazy.CustomizableUI.addWidgetToArea(
        WIDGET_ID,
        lazy.CustomizableUI.AREA_NAVBAR,
        lazy.CustomizableUI.getPlacementOfWidget("urlbar-container").position +
          1
      );
      lazy.BrowserUsageTelemetry.recordWidgetChange(
        WIDGET_ID,
        lazy.CustomizableUI.AREA_NAVBAR,
        "searchpref"
      );
    } else {
      lazy.CustomizableUI.removeWidgetFromArea(WIDGET_ID);
      lazy.BrowserUsageTelemetry.recordWidgetChange(
        WIDGET_ID,
        null,
        "searchpref"
      );
    }
  },

  removePersistedWidths() {
    Services.xulStore.removeValue(
      AppConstants.BROWSER_CHROME_URL,
      "urlbar-container",
      "width"
    );
    Services.xulStore.removeValue(
      AppConstants.BROWSER_CHROME_URL,
      this.WIDGET_ID,
      "width"
    );
    for (let win of lazy.CustomizableUI.windows) {
      let urlbar = win.document.getElementById("urlbar-container");
      urlbar.removeAttribute("width");
      win.document
        .getElementById("nav-bar")
        .querySelectorAll("toolbarspring")
        .forEach(n => n.removeAttribute("width"));
      win.PanelUI.overflowPanel
        .querySelectorAll("toolbarspring")
        .forEach(n => n.removeAttribute("width"));
      let searchbar =
        win.document.getElementById(WIDGET_ID) ||
        win.gNavToolbox.palette.querySelector("#" + WIDGET_ID);
      searchbar.removeAttribute("width");
    }
  },

  get widgetIsInNavBar() {
    let placement = lazy.CustomizableUI.getPlacementOfWidget(WIDGET_ID);
    return placement
      ? placement.area == lazy.CustomizableUI.AREA_NAVBAR
      : false;
  },
};
