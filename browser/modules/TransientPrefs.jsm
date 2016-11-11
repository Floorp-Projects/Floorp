/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TransientPrefs"];

Components.utils.import("resource://gre/modules/Preferences.jsm");

var prefVisibility = new Map;

/* Use for preferences that should only be visible when they've been modified.
   When reset to their default state, they remain visible until restarting the
   application. */

this.TransientPrefs = {
  prefShouldBeVisible: function(prefName) {
    if (Preferences.isSet(prefName))
      prefVisibility.set(prefName, true);

    return !!prefVisibility.get(prefName);
  }
};
