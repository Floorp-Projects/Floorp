/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [];

/*
Scripts written here are executed only once at browser startup.
*/

const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// Check information about startup.
let isFirstRun = false;
let isUpdated = false;
{
    isFirstRun = 
        !Boolean(Services.prefs.getStringPref("browser.startup.homepage_override.mstone", null));

    let nowVersion = AppConstants.MOZ_APP_VERSION_DISPLAY;
    let oldVersionPref = Services.prefs.getStringPref("floorp.startup.oldVersion", null);
    if (oldVersionPref !== nowVersion && !isFirstRun) {
        isUpdated = true;
    }
    Services.prefs.setStringPref("floorp.startup.oldVersion", nowVersion);
}
