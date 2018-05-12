/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "Onboarding", "resource://onboarding/Onboarding.jsm");

const ABOUT_HOME_URL = "about:home";
const ABOUT_NEWTAB_URL = "about:newtab";
const ABOUT_WELCOME_URL = "about:welcome";

// Load onboarding module only when we enable it.
if (Services.prefs.getBoolPref("browser.onboarding.enabled", false)) {
  addEventListener("load", function onLoad(evt) {
    if (!content || evt.target != content.document) {
      return;
    }

    let window = evt.target.defaultView;
    let location = window.location.href;
    if (location == ABOUT_NEWTAB_URL || location == ABOUT_HOME_URL || location == ABOUT_WELCOME_URL) {
      // We just want to run tests as quickly as possible
      // so in the automation test, we don't do `requestIdleCallback`.
      if (Cu.isInAutomation) {
        new Onboarding(this, window);
        return;
      }
      window.requestIdleCallback(() => {
        new Onboarding(this, window);
      });
    }
  }, true);
}
