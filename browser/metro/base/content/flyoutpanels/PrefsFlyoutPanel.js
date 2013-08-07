// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

Components.utils.import("resource://gre/modules/Services.jsm");

let PrefsFlyoutPanel = {
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
      ['PrefsFlyoutPanel',  'prefs-flyoutpanel'],
    ].forEach(function(aElement) {
      let [name, id] = aElement;
      XPCOMUtils.defineLazyGetter(self._elements, name, function() {
        return document.getElementById(id);
      });
    });

    this._prefObserver(null, null, "privacy.donottrackheader.value");
    this._topmostElement = this._elements.PrefsFlyoutPanel;
  },

  _show: function() {
    if (!this._hasShown) {
      SanitizeUI.init();
      this._hasShown = true;

      Services.prefs.addObserver("privacy.donottrackheader.value",
                                 this._prefObserver,
                                 false);
      Services.prefs.addObserver("privacy.donottrackheader.enabled",
                                 this._prefObserver,
                                 false);
    }

    this._topmostElement.show();
  },

  _prefObserver: function(subject, topic, data) {
    let value = -1;
    try {
      value = Services.prefs.getIntPref("privacy.donottrackheader.value");
    } catch(e) {
    }

    let isEnabled = Services.prefs.getBoolPref("privacy.donottrackheader.enabled");

    switch (data) {
      case "privacy.donottrackheader.value":
        // If the user has selected to explicitly tell sites that tracking
        // is OK, or if the user has selected to explicitly tell sites that
        // tracking is NOT OK, we must enable sending the dnt header
        if (((1 == value) || (0 == value)) && !isEnabled) {
          Services.prefs.setBoolPref('privacy.donottrackheader.enabled', true);
        }

        // If the user has made no selection about whether tracking
        // is OK or not, we must diable the dnt header
        if (((1 != value) && (0 != value)) && isEnabled) {
          Services.prefs.setBoolPref('privacy.donottrackheader.enabled', false);
        }
        break;

      case "privacy.donottrackheader.enabled":
        // If someone or something modifies this pref, we should update the
        // other pref as well so our UI doesn't give false information
        if (((1 == value) || (0 == value)) && !isEnabled) {
          Services.prefs.setIntPref('privacy.donottrackheader.value', -1);
        }
        break;
    }
  },
};
