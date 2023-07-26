const ua = "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";

let targetRequestIds = [];

browser.webRequestExt.onBeforeRequest_webpanel_requestId.addListener(function(requestId){
  targetRequestIds.push(requestId);
  setTimeout(function(){
    targetRequestIds = targetRequestIds.filter(id => id !== requestId);
  }, 180 * 1000);
});

browser.webRequest.onBeforeSendHeaders.addListener(
  function (e) {
    if (targetRequestIds.includes(e.requestId)) {
      targetRequestIds = targetRequestIds.filter(id => id !== e.requestId);
      e.requestHeaders.forEach(function (header) {
        if (header.name.toLowerCase() === "user-agent") {
            header.value = ua;
        }
      });
      return {requestHeaders: e.requestHeaders};
    }
  },
  {urls: ["<all_urls>"]},
  ["blocking", "requestHeaders"]
);
