/* Tests for correct behaviour of getEffectiveHost on identity handler */

function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);

  ok(gIdentityHandler, "gIdentityHandler should exist");

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", true).then(() => {
    BrowserTestUtils.addContentEventListener(gBrowser.selectedBrowser, "load", checkResult, true);
    nextTest();
  });
}

// Greek IDN for 'example.test'.
var idnDomain = "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";
var tests = [
  {
    name: "normal domain",
    location: "http://test1.example.org/",
    effectiveHost: "test1.example.org"
  },
  {
    name: "view-source",
    location: "view-source:http://example.com/",
    effectiveHost: null
  },
  {
    name: "normal HTTPS",
    location: "https://example.com/",
    effectiveHost: "example.com",
    isHTTPS: true
  },
  {
    name: "IDN subdomain",
    location: "http://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/",
    effectiveHost: "sub1." + idnDomain
  },
  {
    name: "subdomain with port",
    location: "http://sub1.test1.example.org:8000/",
    effectiveHost: "sub1.test1.example.org"
  },
  {
    name: "subdomain HTTPS",
    location: "https://test1.example.com/",
    effectiveHost: "test1.example.com",
    isHTTPS: true
  },
  {
    name: "view-source HTTPS",
    location: "view-source:https://example.com/",
    effectiveHost: null,
    isHTTPS: true
  },
  {
    name: "IP address",
    location: "http://127.0.0.1:8888/",
    effectiveHost: "127.0.0.1"
  },
];

var gCurrentTest, gCurrentTestIndex = -1, gTestDesc, gPopupHidden;
// Go through the tests in both directions, to add additional coverage for
// transitions between different states.
var gForward = true;
var gCheckETLD = false;
function nextTest() {
  if (!gCheckETLD) {
    if (gForward)
      gCurrentTestIndex++;
    else
      gCurrentTestIndex--;

    if (gCurrentTestIndex == tests.length) {
      // Went too far, reverse
      gCurrentTestIndex--;
      gForward = false;
    }

    if (gCurrentTestIndex == -1) {
      gBrowser.removeCurrentTab();
      finish();
      return;
    }

    gCurrentTest = tests[gCurrentTestIndex];
    gTestDesc = "#" + gCurrentTestIndex + " (" + gCurrentTest.name + ")";
    if (!gForward)
      gTestDesc += " (second time)";
    if (gCurrentTest.isHTTPS) {
      gCheckETLD = true;
    }

    // Navigate to the next page, which will cause checkResult to fire.
    let spec = gBrowser.selectedBrowser.currentURI.spec;
    if (spec == "about:blank" || spec == gCurrentTest.location) {
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, gCurrentTest.location);
    } else {
      // Open the Control Center and make sure it closes after nav (Bug 1207542).
      let popupShown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
      gPopupHidden = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
      gIdentityHandler._identityBox.click();
      info("Waiting for the Control Center to be shown");
      popupShown.then(async () => {
        ok(!BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup), "Control Center is visible");
        // Show the subview, which is an easy way in automation to reproduce
        // Bug 1207542, where the CC wouldn't close on navigation.
        let promiseViewShown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "ViewShown");
        gBrowser.ownerDocument.querySelector("#identity-popup-security-expander").click();
        await promiseViewShown;
        BrowserTestUtils.loadURI(gBrowser.selectedBrowser, gCurrentTest.location);
      });
    }
  } else {
    gCheckETLD = false;
    gTestDesc = "#" + gCurrentTestIndex + " (" + gCurrentTest.name + " without eTLD in identity icon label)";
    if (!gForward)
      gTestDesc += " (second time)";
    gBrowser.selectedBrowser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE |
                                             Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY);
  }
}

function checkResult() {
  if (gBrowser.selectedBrowser.currentURI.spec == "about:blank")
    return;

  // Sanity check other values, and the value of gIdentityHandler.getEffectiveHost()
  is(gIdentityHandler._uri.spec, gCurrentTest.location, "location matches for test " + gTestDesc);
  // getEffectiveHost can't be called for all modes
  if (gCurrentTest.effectiveHost === null) {
    let identityBox = document.getElementById("identity-box");
    ok(identityBox.className == "unknownIdentity" ||
       identityBox.className == "chromeUI", "mode matched");
  } else {
    is(gIdentityHandler.getEffectiveHost(), gCurrentTest.effectiveHost, "effectiveHost matches for test " + gTestDesc);
  }

  if (gPopupHidden) {
    info("Waiting for the Control Center to hide");
    gPopupHidden.then(() => {
      gPopupHidden = null;
      ok(BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup), "Control Center is hidden");
      executeSoon(nextTest);
    });
  } else {
    executeSoon(nextTest);
  }
}
