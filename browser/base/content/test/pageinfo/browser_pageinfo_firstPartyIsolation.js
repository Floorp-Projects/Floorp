const {classes: Cc, interfaces: Ci, manager: Cm} = Components;

function waitForEvent(elem, event) {
  return new Promise(resolve => {
    elem.addEventListener(event, resolve, {capture: true, once: true});
  });
}

function testFirstPartyDomain(pageInfo) {
  return new Promise(resolve => {
    const EXPECTED_DOMAIN = "example.com";
    info("pageInfo load");
    pageInfo.onFinished.push(async function() {
      info("pageInfo onfinished");
      let tree = pageInfo.document.getElementById("imagetree");
      Assert.ok(!!tree, "should have imagetree element");

      // i=0: <img>
      // i=1: <video>
      // i=2: <audio>
      for (let i = 0; i < 3; i++) {
        info("imagetree select " + i);
        tree.view.selection.select(i);
        tree.treeBoxObject.ensureRowIsVisible(i);
        tree.focus();

        let preview = pageInfo.document.getElementById("thepreviewimage");
        info("preview.src=" + preview.src);

        // For <img>, we will query imgIRequest.imagePrincipal later, so we wait
        // for loadend event. For <audio> and <video>, so far we only can get
        // the triggeringprincipal attribute on the node, so we simply wait for
        // loadstart.
        if (i == 0) {
          await waitForEvent(preview, "loadend");
        } else {
          await waitForEvent(preview, "loadstart");
        }

        info("preview load " + i);

        // Originally thepreviewimage is loaded with SystemPrincipal, therefore
        // it won't have origin attributes, now we've changed to loadingPrincipal
        // to the content in bug 1376971, it should have firstPartyDomain set.
        if (i == 0) {
          let req = preview.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
          Assert.equal(req.imagePrincipal.originAttributes.firstPartyDomain, EXPECTED_DOMAIN,
                       "imagePrincipal should have firstPartyDomain set to " + EXPECTED_DOMAIN);
        }

        // Check the node has the attribute 'triggeringprincipal'.
        let serial = Components.classes["@mozilla.org/network/serialization-helper;1"]
                               .getService(Components.interfaces.nsISerializationHelper);
        let loadingPrincipalStr = preview.getAttribute("triggeringprincipal");
        let loadingPrincipal = serial.deserializeObject(loadingPrincipalStr);
        Assert.equal(loadingPrincipal.originAttributes.firstPartyDomain, EXPECTED_DOMAIN,
                     "loadingPrincipal should have firstPartyDomain set to " + EXPECTED_DOMAIN);

      }

      resolve();
    });
  });
}

async function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.loadURI("https://example.com/browser/browser/base/content/test/pageinfo/image.html");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let spec = gBrowser.selectedBrowser.currentURI.spec;

  // Pass a dummy imageElement, if there isn't an imageElement, pageInfo.js
  // will do a preview, however this sometimes will cause intermittent failures,
  // see bug 1403365.
  let pageInfo = BrowserPageInfo(spec, "mediaTab", {});
  info("waitForEvent pageInfo");
  await waitForEvent(pageInfo, "load");

  info("calling testFirstPartyDomain");
  await testFirstPartyDomain(pageInfo);

  pageInfo.close();
  gBrowser.removeCurrentTab();
  finish();
}
