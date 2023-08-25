"use strict";
var ua;
var dispver;

async function getua() {
  var defaultua = navigator.userAgent;
  var replua = defaultua.substring(defaultua.indexOf('Firefox/'));
  return browser.BrowserInfo.getDisplayVersion()
  .then(data => {
    var dispve = data;
    dispver = dispve.replace(/ /g, "-");
    ua = defaultua.replace(replua, "Floorp/"+dispver);
    console.log(ua);
    return ua;
  })
}
async function rewriteUserAgentHeader(e) {
  var ua = await getua()
  for (var header of e.requestHeaders) {
    if (header.name.toLowerCase() === "user-agent") {
      header.value = ua;
    }
  }
  return {requestHeaders: e.requestHeaders};
}

browser.webRequest.onBeforeSendHeaders.addListener(rewriteUserAgentHeader,
                                          {urls: ["https://ablaze.one/*","https://floorp.ablaze.one/*", "https://floorp.app/*", "https://support.ablaze.one/", "https://ss1.xrea.com/menkuri.s270.xrea.com/*"]},
                                          ["blocking", "requestHeaders"]);
