"use strict";

// For some about: URLs, they will take more time to load and cause timeout.
// See Bug 1270998.
requestLongerTimeout(2);

add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["signon.management.page.enabled", true],
  ]});

  let aboutURLs = [];

  // List of about: URLs that may cause problem, so we skip them in this test.
  let skipURLs = [
    // about:addons triggers an assertion in NS_CompareLoadInfoAndLoadContext:
    // "The value of mUserContextId in the loadContext and in the loadInfo are not the same"
    // This is due to a fetch request that has the default user context. Since
    // the fetch request omits credentials, the user context doesn't matter.
    "addons",
    // about:credits will initiate network request.
    "credits",
    // about:telemetry will fetch Telemetry asynchronously and takes longer,
    // so we skip this for now.
    "telemetry",
    // about:downloads causes a shutdown leak with stylo-chrome. bug 1419943.
    "downloads",
    // about:debugging requires specific wait code for internal pending RDP requests.
    "debugging",
    "debugging-new",
  ];

  for (let cid in Cc) {
    let result = cid.match(/@mozilla.org\/network\/protocol\/about;1\?what\=(.*)$/);
    if (!result) {
      continue;
    }

    let aboutType = result[1];
    let contract = "@mozilla.org/network/protocol/about;1?what=" + aboutType;
    try {
      let am = Cc[contract].getService(Ci.nsIAboutModule);
      let uri = Services.io.newURI("about:" + aboutType);
      let flags = am.getURIFlags(uri);
      if (!(flags & Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT) &&
          !skipURLs.includes(aboutType)) {
        aboutURLs.push(aboutType);
      }
    } catch (e) {
      // getService might have thrown if the component doesn't actually
      // implement nsIAboutModule
    }
  }

  for (let url of aboutURLs) {
    info("Loading about:" + url);
    let tab = BrowserTestUtils.addTab(gBrowser, "about:" + url, {userContextId: 1});
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    ok(true, "Done loading about:" + url);

    BrowserTestUtils.removeTab(tab);
  }
});
