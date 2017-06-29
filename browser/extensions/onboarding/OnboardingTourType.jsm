/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["OnboardingTourType"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

var OnboardingTourType = {
  /**
   * Determine the current tour type (new user tour or update user tour).
   * The function checks 2 criterias
   *  - TOURSET_VERSION: current onboarding tourset version
   *  - PREF_SEEN_TOURSET_VERSION: the user seen tourset version
   * As the result the function will set the right current tour type in the tour type pref (PREF_TOUR_TYPE) for later use.
   */
  check() {
    const PREF_TOUR_TYPE = "browser.onboarding.tour-type";
    const PREF_SEEN_TOURSET_VERSION = "browser.onboarding.seen-tourset-version";
    const TOURSET_VERSION = Services.prefs.getIntPref("browser.onboarding.tourset-version");

    if (!Services.prefs.prefHasUserValue(PREF_SEEN_TOURSET_VERSION)) {
      // User has never seen an onboarding tour, present the user with the new user tour.
      Services.prefs.setStringPref(PREF_TOUR_TYPE, "new");
    } else if (Services.prefs.getIntPref(PREF_SEEN_TOURSET_VERSION) < TOURSET_VERSION) {
      // show the update user tour when tour set version is larger than the seen tourset version
      Services.prefs.setStringPref(PREF_TOUR_TYPE, "update");
      Services.prefs.setBoolPref("browser.onboarding.hidden", false);
    }
    Services.prefs.setIntPref(PREF_SEEN_TOURSET_VERSION, TOURSET_VERSION);
  },
};
