{
    const FLOORP_WEBCOMPAT_ENABLED_PREF = "floorp.webcompat.enabled";

    const WEBCOMPATS_USERAGENT = [
        {
            "matches": [
                "*://*.mercari.com/*",
                "*://*.mercari.jp/*",
            ],
            "ua": FIREFOX_STABLE_UA,
        },
        {
            "matches": [
                "*://*.pococha.com/*",
                "*://*.recochoku.jp/*",
                "*://business.apple.com/*",
                "*://*.picnet.co.jp/*",
                "*://*.picnet8.jp/*",
            ],
            "ua": CHROME_STABLE_UA,
        },
        {
            "matches": [
                "*://*.bing.com/*", // This make Bing search engine's chat work.
            ],
            "ua": MSEDGE_STABLE_UA,
        },
    ];


    // It must be a synchronous process in order to process requests before the page is loaded.
    let enabled = browser.aboutConfigPrefs.getBoolPref(FLOORP_WEBCOMPAT_ENABLED_PREF);
    browser.aboutConfigPrefs.onPrefChange.addListener(function() {
        enabled = browser.aboutConfigPrefs.getBoolPref(FLOORP_WEBCOMPAT_ENABLED_PREF);
    }, FLOORP_WEBCOMPAT_ENABLED_PREF);
    let platformInfo = browser.runtime.getPlatformInfo();

    for (let WEBCOMPAT_USERAGENT of WEBCOMPATS_USERAGENT) {
        browser.webRequest.onBeforeSendHeaders.addListener(
            async function(e) {
                if (!await enabled) return;
                if (e.tabId === -1) return;
                if (typeof WEBCOMPAT_USERAGENT["ua"][(await platformInfo).os] === "undefined") return;
                for (let requestHeader of e.requestHeaders) {
                    if (requestHeader.name.toLowerCase() === "user-agent") {
                        requestHeader.value = WEBCOMPAT_USERAGENT["ua"][(await platformInfo).os];
                    }
                }
                return { requestHeaders: e.requestHeaders };
            },
            { urls: WEBCOMPAT_USERAGENT["matches"] },
            ["blocking", "requestHeaders"]
        );
    }
}
