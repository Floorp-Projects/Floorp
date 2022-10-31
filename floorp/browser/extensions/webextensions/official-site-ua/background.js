const targetURL = "<all_urls>";
const targetUA = "Mozilla/5.0 (Android 12; Mobile; rv:102.0) Gecko/102.0 Firefox/102.0";

function rewriteUserAgentHeader(e) {
    if (e.tabId !== -1) {
        return;
    }
    if (e.documentUrl === undefined && e.type !== "main_frame") {
        return;
    }
    e.requestHeaders.forEach(function (header) {
        if (header.name.toLowerCase() === "user-agent") {
            header.value = targetUA;
        }
    });
    return { requestHeaders: e.requestHeaders };
}

browser.webRequest.onBeforeSendHeaders.addListener(
    rewriteUserAgentHeader,
    { urls: [targetURL] },
    ["blocking", "requestHeaders"]
);
