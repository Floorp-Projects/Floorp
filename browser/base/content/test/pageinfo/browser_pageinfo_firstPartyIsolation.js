const Cm = Components.manager;

async function testFirstPartyDomain(pageInfo) {
  const EXPECTED_DOMAIN = "example.com";
  await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");
  info("pageInfo initialized");
  let tree = pageInfo.document.getElementById("imagetree");
  Assert.ok(!!tree, "should have imagetree element");

  // i=0: <img>
  // i=1: <video>
  // i=2: <audio>
  for (let i = 0; i < 3; i++) {
    info("imagetree select " + i);
    tree.view.selection.select(i);
    tree.ensureRowIsVisible(i);
    tree.focus();

    let preview = pageInfo.document.getElementById("thepreviewimage");
    info("preview.src=" + preview.src);

    // For <img>, we will query imgIRequest.imagePrincipal later, so we wait
    // for load event. For <audio> and <video>, so far we only can get
    // the triggeringprincipal attribute on the node, so we simply wait for
    // loadstart.
    if (i == 0) {
      await BrowserTestUtils.waitForEvent(preview, "load");
    } else {
      await BrowserTestUtils.waitForEvent(preview, "loadstart");
    }

    info("preview load " + i);

    // Originally thepreviewimage is loaded with SystemPrincipal, therefore
    // it won't have origin attributes, now we've changed to loadingPrincipal
    // to the content in bug 1376971, it should have firstPartyDomain set.
    if (i == 0) {
      let req = preview.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
      Assert.equal(
        req.imagePrincipal.originAttributes.firstPartyDomain,
        EXPECTED_DOMAIN,
        "imagePrincipal should have firstPartyDomain set to " + EXPECTED_DOMAIN
      );
    }

    // Check the node has the attribute 'triggeringprincipal'.
    let loadingPrincipalStr = preview.getAttribute("triggeringprincipal");
    let loadingPrincipal = E10SUtils.deserializePrincipal(loadingPrincipalStr);
    Assert.equal(
      loadingPrincipal.originAttributes.firstPartyDomain,
      EXPECTED_DOMAIN,
      "loadingPrincipal should have firstPartyDomain set to " + EXPECTED_DOMAIN
    );
  }
}

async function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });

  let url =
    "https://example.com/browser/browser/base/content/test/pageinfo/image.html";
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
  await loadPromise;

  // Pass a dummy imageElement, if there isn't an imageElement, pageInfo.js
  // will do a preview, however this sometimes will cause intermittent failures,
  // see bug 1403365.
  let pageInfo = BrowserPageInfo(url, "mediaTab", {});
  info("waitForEvent pageInfo");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");

  info("calling testFirstPartyDomain");
  await testFirstPartyDomain(pageInfo);

  pageInfo.close();
  gBrowser.removeCurrentTab();
  finish();
}
