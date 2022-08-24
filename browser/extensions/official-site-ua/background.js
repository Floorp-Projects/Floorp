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
      header.value = ua;
    }
  return {requestHeaders: e.requestHeaders};
}

browser.webRequest.onBeforeSendHeaders.addListener(rewriteUserAgentHeader,
                                          {urls: ["https://ablaze.one/*","https://floorp.ablaze.one/*", "https://support.ablaze.one/"]},
                                          ["blocking", "requestHeaders"]);
                                          