const FIREFOX_STABLE_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/110.0";
const CHROME_STABLE_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36";

const WEBCOMPATS_USERAGENT = [
    {
        "matches": ["*://*.mercari.com/*"],
        "ua": FIREFOX_STABLE_UA,
        "platforms": ["all"], // "mac", "win", "android", "cros", "linux", "openbsd", "fuchsia", "all"
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
        "platforms": ["all"],
    },
];

for (let WEBCOMPAT_USERAGENT of WEBCOMPATS_USERAGENT) {
    if (WEBCOMPAT_USERAGENT.platforms.includes("all") ||
        WEBCOMPAT_USERAGENT.platforms.includes(platform)) {
        browser.webRequest.onBeforeSendHeaders.addListener(
            function(e) {
                if (e.tabId === -1) return;
                for (let requestHeader of e.requestHeaders) {
                    if (requestHeader.name.toLowerCase() === "user-agent") {
                        requestHeader.value = WEBCOMPAT_USERAGENT["ua"];
                    }
                }
                return { requestHeaders: e.requestHeaders };
            },
            { urls: WEBCOMPAT_USERAGENT["matches"] },
            ["blocking", "requestHeaders"]
        );
    }
}
