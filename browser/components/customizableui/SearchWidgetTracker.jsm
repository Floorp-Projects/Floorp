/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Keeps the "browser.search.widget.inNavBar" preference synchronized.
 */

"use strict";

var EXPORTED_SYMBOLS = ["SearchWidgetTracker"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);

const WIDGET_ID = "search-container";
const PREF_NAME = "browser.search.widget.inNavBar";

const SearchWidgetTracker = {
  init() {
    this.onWidgetAdded = this.onWidgetRemoved = (widgetId, area) => {
      if (widgetId == WIDGET_ID && area == CustomizableUI.AREA_NAVBAR) {
        this.syncPreferenceWithWidget();
      }
    };
    this.onWidgetReset = this.onWidgetUndoMove = node => {
      if (node.id == WIDGET_ID) {
        this.syncPreferenceWithWidget();
      }
    };
    CustomizableUI.addListener(this);
    Services.prefs.addObserver(PREF_NAME, () =>
      this.syncWidgetWithPreference()
    );
  },

  onAreaNodeRegistered(aArea) {
    // The placement of the widget always takes priority, and the preference
    // should always match the actual placement when the browser starts up - i.e.
    // once the navigation bar has been registered.
    if (aArea == CustomizableUI.AREA_NAVBAR) {
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
      CustomizableUI.addWidgetToArea(
        WIDGET_ID,
        CustomizableUI.AREA_NAVBAR,
        CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1
      );
    } else {
      CustomizableUI.removeWidgetFromArea(WIDGET_ID);
    }
  },

  get widgetIsInNavBar() {
    let placement = CustomizableUI.getPlacementOfWidget(WIDGET_ID);
    return placement ? placement.area == CustomizableUI.AREA_NAVBAR : false;
  },
};
