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
const { AddonManager } = ChromeUtils.import(
    "resource://gre/modules/AddonManager.jsm"
);

function Listener() {
    Services.obs.removeObserver(Listener, "final-ui-startup");
    (async() => {
        async function handle() {
            let tabSleepEnabledPref = Services.prefs.getBoolPref("floorp.tabsleep.enabled");
            let addon = await AddonManager.getAddonByID("tab-sleep@floorp.ablaze.one");
            if (addon) {
                if (tabSleepEnabledPref) {
                    await addon.enable({ allowSystemAddons: true });
                } else {
                    await addon.disable({ allowSystemAddons: true });
                }
            }
        }
        await handle();
        Services.prefs.addObserver("floorp.tabsleep.enabled", handle);
    })();
}
Services.obs.addObserver(Listener, "final-ui-startup");
