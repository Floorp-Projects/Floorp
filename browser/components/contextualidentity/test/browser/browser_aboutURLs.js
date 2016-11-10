"use strict";

// For some about: URLs, they will take more time to load and cause timeout.
// See Bug 1270998.
requestLongerTimeout(2);

add_task(function* () {
  let aboutURLs = [];

  // List of about: URLs that will initiate network requests.
  let networkURLs = [
    "credits",
    "telemetry" // about:telemetry will fetch Telemetry asynchrounously and takes
                // longer, we skip this for now.
  ];

  let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  for (let cid in Cc) {
    let result = cid.match(/@mozilla.org\/network\/protocol\/about;1\?what\=(.*)$/);
    if (!result) {
      continue;
    }

    let aboutType = result[1];
    let contract = "@mozilla.org/network/protocol/about;1?what=" + aboutType;
    try {
      let am = Cc[contract].getService(Ci.nsIAboutModule);
      let uri = ios.newURI("about:" + aboutType, null, null);
      let flags = am.getURIFlags(uri);
      if (!(flags & Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT) &&
          networkURLs.indexOf(aboutType) == -1) {
        aboutURLs.push(aboutType);
      }
    } catch (e) {
      // getService might have thrown if the component doesn't actually
      // implement nsIAboutModule
    }
  }

  for (let url of aboutURLs) {
    info("Loading about:" + url);
    let tab = gBrowser.addTab("about:" + url, {userContextId: 1});
    yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    ok(true, "Done loading about:" + url);

    yield BrowserTestUtils.removeTab(tab);
  }
});
