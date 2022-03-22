async function FLOORP_PROT_LV2() {
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/");
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", false);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	false);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		true);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", true);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			false);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		false);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	false);
    console.log("Configuration successfully set to LV2." );
}

async function FLOORP_PROT_DEFAULT() {
    browser.aboutConfigPrefs.setBoolPref("dom.enable_performance", true);
    browser.aboutConfigPrefs.setBoolPref("dom.enable_user_timing",	true);
    browser.aboutConfigPrefs.setBoolPref("geo.enabled", true);
    browser.aboutConfigPrefs.setCharPref("geo.wifi.uri", "");
    browser.aboutConfigPrefs.setBoolPref("geo.wifi.logging.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.mozTCPSocket.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.netinfo.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.network.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.default_address_only",	false);
    browser.aboutConfigPrefs.setBoolPref("media.peerconnection.ice.no_host",			false);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.navigator.video.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.screensharing.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("media.getusermedia.audiocapture.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.battery.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.send_pings.require_same_host",		false);
    browser.aboutConfigPrefs.setIntPref("dom.maxHardwareConcurrency",	2);
    browser.aboutConfigPrefs.setBoolPref("browser.search.geoSpecificDefaults", true);
    browser.aboutConfigPrefs.setBoolPref("keyword.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("browser.urlbar.trimURLs", true);
    browser.aboutConfigPrefs.setBoolPref("security.fileuri.strict_origin_policy", false);
    browser.aboutConfigPrefs.setBoolPref("media.video_stats.enabled", true);
    browser.aboutConfigPrefs.setBoolPref("dom.ipc.plugins.reportCrashURL",			true);
    browser.aboutConfigPrefs.setCharPref("extensions.blocklist.url", "");
    browser.aboutConfigPrefs.setBoolPref("extensions.blocklist.enabled",			false);
    browser.aboutConfigPrefs.setBoolPref("services.blocklist.update_enabled",			false);
    browser.aboutConfigPrefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr",	true);
    browser.aboutConfigPrefs.setBoolPref("network.allow-experiments", true);
    browser.aboutConfigPrefs.setBoolPref("browser.tabs.crashReporting.sendReport",		true);
    browser.aboutConfigPrefs.setBoolPref("browser.crashReports.unsubmittedCheck.enabled",	true);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.userContext.enabled", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting", false);
    browser.aboutConfigPrefs.setBoolPref("browser.chrome.site_icons", true);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode", false);
    browser.aboutConfigPrefs.setBoolPref("dom.security.https_only_mode_ever_enabled", false);
    browser.aboutConfigPrefs.setBoolPref("browser.privatebrowsing.autostart", false);
    browser.aboutConfigPrefs.setBoolPref("privacy.resistFingerprinting.block_mozAddonManager", false);
    console.log("Successfully reset the configuration." );
}

function delay(sec){
    return new Promise(function(resolve){
        setTimeout(resolve, sec * 1000);
    });
}

window.onload = () =>{
    (async() => {
        await delay(3);
        var floorpPL = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel");
        if (floorpPL == 2) {
            browser.aboutConfigPrefs.setCharPref("floorp.protection.protectlevel", "2");
            var floorpPL = await browser.aboutConfigPrefs.getPref("floorp.protection.protectlevel");
            console.log("floorp.protection.protectlevel-startevent: " + floorpPL);
            FLOORP_PROT_DEFAULT();
            FLOORP_PROT_LV2();
        }
    })();
}