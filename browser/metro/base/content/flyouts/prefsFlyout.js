// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

Components.utils.import("resource://gre/modules/Services.jsm");

let PrefsFlyout = {
  _isInitialized: false,
  _hasShown: false,
  init: function pv_init() {
    if (this._isInitialized) {
      Cu.reportError("Attempting to re-initialize PreferencesPanelView");
      return;
    }

    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
    this._isInitialized = true;
    let self = this;

    this._elements = {};
    [
      ['prefsFlyout',  'prefs-flyoutpanel'],
      ['dntNoPref',    'prefs-dnt-nopref'],
      ['telemetryPref','prefs-telemetry'],
    ].forEach(function(aElement) {
      let [name, id] = aElement;
      XPCOMUtils.defineLazyGetter(self._elements, name, function() {
        return document.getElementById(id);
      });
    });

    this._topmostElement = this._elements.prefsFlyout;
  },

  _show: function() {
    if (!this._hasShown) {
      SanitizeUI.init();
      this._hasShown = true;
    }

    this._elements.prefsFlyout.show();
  },

  onDNTPreferenceChanged: function onDNTPreferenceChanged() {
    let selected = this._elements.dntNoPref.selected;

    // When "tell sites nothing about my preferences" is selected, disable do not track.
    Services.prefs.setBoolPref("privacy.donottrackheader.enabled", !selected);
  },

  onTelemetryPreferenceChanged: function onTelemetryPreferenceChanged(aBool) {
    Services.prefs.setBoolPref("toolkit.telemetry.enabled", aBool);
  }
};
