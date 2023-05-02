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
const { OS } = ChromeUtils.import(
    "resource://gre/modules/osfile.jsm"
);
const { setTimeout, setInterval, clearTimeout, clearInterval } = ChromeUtils.import(
    "resource://gre/modules/Timer.jsm"
);
const { FileUtils } = ChromeUtils.import(
    "resource://gre/modules/FileUtils.jsm"
);
const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
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

const isMainBrowser = env.get("MOZ_BROWSER_TOOLBOX_PORT") === "";


// Optimize the notification function.
{
    let isNativeNotificationEnabled = false;
    if (AppConstants.platform === "win") {
        let version = Services.sysinfo.getProperty("version");
        if (version === "10.0") {
            isNativeNotificationEnabled = true;
        }
    } else if (AppConstants.platform === "linux") {
        isNativeNotificationEnabled = true;
    }
    let prefs = Services.prefs.getDefaultBranch(null);
    prefs.setBoolPref("alerts.useSystemBackend", isNativeNotificationEnabled);
}


async function onFinalUIStartup() {
    Services.obs.removeObserver(onFinalUIStartup, "final-ui-startup");
    const { CustomizableUI } = ChromeUtils.import(
        "resource:///modules/CustomizableUI.jsm"
    );
    let { BrowserManagerSidebar } = ChromeUtils.import("resource:///modules/BrowserManagerSidebar.jsm")
    BrowserManagerSidebar.prefsUpdate()

    IOUtils.exists(OS.Path.join(OS.Constants.Path.profileDir, "newtabImages"))
        .then((data) => {
            if(!data) IOUtils.makeDirectory(OS.Path.join(OS.Constants.Path.profileDir, "newtabImages"))
        })

    // Write CSS.
    
    IOUtils.exists(OS.Path.join(OS.Constants.Path.profileDir, "chrome")).then((data) => {
        if (!data) {
            let userChromecssPath = OS.Path.join(OS.Constants.Path.profileDir, "chrome");
            let uccpth = OS.Path.join(userChromecssPath, 'userChrome.css')
            IOUtils.writeUTF8(uccpth,`
/*************************************************************************************************************************************************************************************************************************************************************

"userChrome.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's interface (NOT internal site) using "chrome" privileges.
For instance, if you want to hide the tab bar, you can use the following CSS rule:

**************************************
#TabsToolbar {                       *
    display: none !important;        *
}                                    *
**************************************

NOTE: You can use the userChrome.css file without change preferences (about:config)

Quote: https://userChrome.org | https://github.com/topics/userchrome 

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
@-moz-document url(chrome://browser/content/browser.xhtml) {
/* Please write your custom CSS under this line*/


}
`);

            let ucconpth = OS.Path.join(userChromecssPath, 'userContent.css')
            IOUtils.writeUTF8(ucconpth,`
/*************************************************************************************************************************************************************************************************************************************************************
 
"userContent.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's intenal site using "chrome" privileges.
For instance, if you want to apply CSS at "about:newtab" and "about:home", you can use the following CSS rule:

***********************************************************************
@-moz-document url-prefix("about:newtab"), url-prefix("about:home") { *
                                                                      *
/*Write your css*/                                                    *
                                                                      *
}                                                                     *
***********************************************************************

NOTE: You can use the userContent.css file without change preferences (about:config)

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
/* Please write your custom CSS under this line*/
`);

        }
    });

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


    // Setup for Undo Close Tab
    if (isFirstRun) {
        CustomizableUI.addWidgetToArea("undo-closed-tab", CustomizableUI.AREA_NAVBAR, -1);
    }


    // Migrate from "floorp.optimized.msbutton.ope" pref
    if (Services.prefs.prefHasUserValue("floorp.optimized.msbutton.ope")) {
        CustomizableUI.removeWidgetFromArea("forward-button");
        CustomizableUI.removeWidgetFromArea("back-button");
        Services.prefs.clearUserPref("floorp.optimized.msbutton.ope");
    }


    if (isFirstRun) {
        setTimeout(() => {
            Services.prefs.setStringPref("browser.contentblocking.category", "strict")
        }, 5000);
    }
}
if (isMainBrowser) {
    Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
}


// Optimize for portable version
if (Services.prefs.getBoolPref("floorp.isPortable", false)) {
    // from firefox-scripts (https://github.com/xiaoxiaoflood/firefox-scripts/blob/2bf4334a604ef2184657ed1ddf512bbe0cd7c63c/chrome/utils/BootstrapLoader.jsm#L19-L22)
    const Constants = ChromeUtils.import('resource://gre/modules/AppConstants.jsm');
    const temp = Object.assign({}, Constants.AppConstants);
    temp.MOZ_UPDATER = false;
    Constants.AppConstants = Object.freeze(temp);

    // https://searchfox.org/mozilla-esr102/source/toolkit/modules/UpdateUtils.jsm#397
    // https://searchfox.org/mozilla-esr102/source/toolkit/modules/UpdateUtils.jsm#506-507
    // https://searchfox.org/mozilla-esr102/source/toolkit/modules/UpdateUtils.jsm#557,573
    const UpdateUtils = ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm");
    UpdateUtils.UpdateUtils.PER_INSTALLATION_PREFS["app.update.auto"].policyFn =
        function() { return false };
    UpdateUtils.UpdateUtils.PER_INSTALLATION_PREFS["app.update.background.enabled"].policyFn =
        function() { return false };
}


// Set BMS icon provider
{
    let os_languages = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;
    let isChina = os_languages.includes("zh-CN");
    Services.prefs.getDefaultBranch(null)
        .setStringPref(
            "floorp.browser.sidebar.useIconProvider",
            isChina ?
                "yandex" : // Setup for China
                "duckduckgo"
        );
}


if (isMainBrowser) {
    // Load Tab Sleep feature
    try {
        ChromeUtils.import("resource:///modules/TabSleep.jsm");
    } catch (e) { console.error(e) }

    // Load OpenLinkInExternal feature
    try {
        // Disable it in the Flatpak version because it does not work.
        // https://gitlab.gnome.org/GNOME/gtk/-/blob/4300a5c609306ce77cbc8a3580c19201dccd8d13/gdk/gdk.c#L472
        if (AppConstants.platform === "linux" && FileUtils.File("/.flatpak-info").exists()) {
            Services.prefs.lockPref("floorp.openLinkInExternal.enabled");
        }
        if (AppConstants.platform === "win" || AppConstants.platform === "linux") {
            if (Services.prefs.getBoolPref("floorp.openLinkInExternal.enabled", false)) {
                ChromeUtils.import("resource:///modules/OpenLinkInExternal.jsm");
            }
        }
    } catch (e) { console.error(e) }
}
