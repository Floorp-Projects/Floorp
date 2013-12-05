// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

let SearchFlyoutPanel = {
  _isInitialized: false,
  _hasShown: false,
  init: function pv_init() {
    if (this._isInitialized) {
      Cu.reportError("Attempting to re-initialize PreferencesPanelView");
      return;
    }
    this._topmostElement = document.getElementById("search-flyoutpanel");
    this._isInitialized = true;
  },

  checked: function checked(aId) {
    aId = aId.replace("search-", "");
    Services.search.defaultEngine = this._engines[parseInt(aId)];
    this.updateSearchEngines();
  },

  updateSearchEngines: function () {
    // Clear the list
    let setting = document.getElementById("search-options");
    while(setting.hasChildNodes()) {
      setting.removeChild(setting.firstChild);
    }

    // Build up the list and check the default
    this._engines = Services.search.getVisibleEngines();
    let defaultEngine = Services.search.defaultEngine;

    this._engines.forEach(function (aEngine, aIndex) {
      let radio = document.createElementNS(XUL_NS, "radio");
      radio.setAttribute("id", "search-" + aIndex);
      radio.setAttribute("label", aEngine.name);
      radio.setAttribute("oncommand", "FlyoutPanelsUI.SearchFlyoutPanel.checked(this.id)");
      if (defaultEngine == aEngine) {
        radio.setAttribute("selected", true);
      }
      setting.appendChild(radio);
    }.bind(this));
  },

  _show: function() {
    if (!this._hasShown) {
      this._hasShown = true;
      this.updateSearchEngines();
    }
    this._topmostElement.show();
  }
};
