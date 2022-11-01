const ua = "Mozilla/5.0 (Android 12; Mobile; rv:102.0) Gecko/102.0 Firefox/102.0";

browser.webRequestExt.onBeforeRequest_webpanel_requestId.addListener(function(requestId){
  function listener(e) {
    if (e.requestId == requestId) {
      browser.webRequest.onBeforeSendHeaders.removeListener(listener);
      e.requestHeaders.forEach(function (header) {
        if (header.name.toLowerCase() === "user-agent") {
            header.value = ua;
        }
      });
      return {requestHeaders: e.requestHeaders};
    }
  }
  browser.webRequest.onBeforeSendHeaders.addListener(
    listener,
    {urls: ["<all_urls>"]},
    ["blocking", "requestHeaders"]
  );
  setTimeout(function(){
    if (browser.webRequest.onBeforeSendHeaders.hasListener(listener)) {
      browser.webRequest.onBeforeSendHeaders.removeListener(listener);
    }
  }, 180 * 1000);
})
