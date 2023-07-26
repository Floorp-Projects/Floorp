/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["isFirstRun","isUpdated","isMainBrowser"];

/*
Scripts written here are executed only once at browser startup.
*/

const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
);
const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
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
const { CustomizableUI } = ChromeUtils.import(
    "resource:///modules/CustomizableUI.jsm"
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
    let { BrowserManagerSidebar } = ChromeUtils.import("resource:///modules/BrowserManagerSidebar.jsm");
    BrowserManagerSidebar.prefsUpdate();

    IOUtils.exists(OS.Path.join(OS.Constants.Path.profileDir, "newtabImages"))
        .then((data) => {
            if(!data) IOUtils.makeDirectory(OS.Path.join(OS.Constants.Path.profileDir, "newtabImages"))
        })

    // Write CSS.
    IOUtils.exists(OS.Path.join(OS.Constants.Path.profileDir, "chrome")).then((data) => {
        if (!data) {
            let userChromecssPath = OS.Path.join(OS.Constants.Path.profileDir, "chrome");
            let uccpth = OS.Path.join(userChromecssPath, 'userChrome.css')
            IOUtils.writeUTF8(uccpth, `
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
            IOUtils.writeUTF8(ucconpth, `
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

    if(isUpdated && Services.prefs.getBoolPref("floorp.enable.multitab")){
        Services.prefs.setBoolPref("floorp.tabbar.style",1)
        Services.prefs.deleteBranch("floorp.tabbar.style")
    }

    if(isFirstRun){
        setTimeout(() => {
            let fxViewButton = CustomizableUI.getWidget("firefox-view-button");
            if (fxViewButton) {
                CustomizableUI.moveWidgetWithinArea("firefox-view-button", 1000);
            } else {
                console.error("Can't find the Firefox (Floorp) View button.");
            }
        }, 2000);
    }
}
if (isMainBrowser) {
    Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
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
                "google"
        );
}

// delete BMS's sidebar extension panel preference
{
    let pref = "floorp.extensions.webextensions.sidebar-action";
    if (Services.prefs.prefHasUserValue(pref)) {
        Services.prefs.clearUserPref(pref);
    }
}

if (isMainBrowser) {
    // Load actors
    try {
        ChromeUtils.import("resource:///modules/FloorpActors.jsm");
    } catch (e) { console.error(e) }

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

    // Load PortableUpdate feature
    try {
        if (Services.prefs.getBoolPref("floorp.isPortable", false)) {
            ChromeUtils.import("resource:///modules/PortableUpdate.jsm");
        }
    } catch (e) { console.error(e) }
}
