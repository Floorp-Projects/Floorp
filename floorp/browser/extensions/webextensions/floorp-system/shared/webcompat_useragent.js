const FLOORP_WEBCOMPAT_ENABLED_PREF = "floorp.webcompat.enabled";

const FIREFOX_STABLE_UA = {
    "win": "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/110.0",
    "linux": "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/110.0",
    "mac": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:109.0) Gecko/20100101 Firefox/110.0",
}
const CHROME_STABLE_UA = {
    "win": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36",
    "linux": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36",
    "mac": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36"
};

const WEBCOMPATS_USERAGENT = [
    {
        "matches": ["*://*.mercari.com/*"],
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
];


// Synchronization is required to process requests before the page is loaded.
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
