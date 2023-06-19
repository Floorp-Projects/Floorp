/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var prefVisibility = new Map();

/* Use for preferences that should only be visible when they've been modified.
   When reset to their default state, they remain visible until restarting the
   application. */

export var TransientPrefs = {
  prefShouldBeVisible(prefName) {
    if (Services.prefs.prefHasUserValue(prefName)) {
      prefVisibility.set(prefName, true);
    }

    return !!prefVisibility.get(prefName);
  },
};
