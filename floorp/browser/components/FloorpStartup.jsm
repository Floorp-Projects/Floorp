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
const { AddonManager } = ChromeUtils.import(
    "resource://gre/modules/AddonManager.jsm"
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


async function onFinalUIStartup() {
    Services.obs.removeObserver(onFinalUIStartup, "final-ui-startup");

    if (isFirstRun) {
        try {
            let url = "https://addons.mozilla.org/firefox/downloads/latest/Gesturefy/latest.xpi" 
            let install = await AddonManager.getInstallForURL(url);
            await install.install();
        } catch (e) { console.error(e) }
        try {
            let url = "https://addons.mozilla.org/firefox/downloads/latest/ublock-origin/latest.xpi" 
            let install = await AddonManager.getInstallForURL(url);
            let installed = await install.install();
            await installed.disable(); // Default is disabled.
        } catch (e) { console.error(e) }
    }

    try {
        if (Services.prefs.getBoolPref("floorp.extensions.translate.migrateFromSystemAddonToUserAddon.ended", false)) return;
        let addon = await AddonManager.getAddonByID("{036a55b4-5e72-4d05-a06c-cba2dfcc134a}");
        if (addon === null || addon.version === "1.0.0") {
            let url = "https://addons.mozilla.org/firefox/downloads/latest/traduzir-paginas-web/latest.xpi";
            let install = await AddonManager.getInstallForURL(url);
            let installed = await install.install();
            await installed.reload(); // Do not show addon release note.
        }
        Services.prefs.setBoolPref("floorp.extensions.translate.migrateFromSystemAddonToUserAddon.ended", true);
    } catch (e) { console.error(e) }
}
Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
