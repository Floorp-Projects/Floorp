const ua = "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Mobile Safari/537.36 Edg/108.0.1462.46";

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
