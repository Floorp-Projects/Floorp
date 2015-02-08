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
    this._onPrefChanged = this._onPrefChanged.bind(this);

    this.optionsView = new OptionsView({
      branchName: BRANCH_NAME,
      menupopup: $("#performance-options-menupopup")
    });

    yield this.optionsView.initialize();
    this.optionsView.on("pref-changed", this._onPrefChanged);
  }),

  /**
   * Unbinds events and cleans up view.
   */
  destroy: function () {
    this.optionsView.off("pref-changed", this._onPrefChanged);
    this.optionsView.destroy();
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
