/* Tests for correct behaviour of getEffectiveHost on identity handler */
function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);

  ok(gIdentityHandler, "gIdentityHandler should exist");

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", checkResult, true);

  nextTest();
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
    location: "http://sub1." + idnDomain + "/",
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
]

var gCurrentTest, gCurrentTestIndex = -1, gTestDesc;
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
      gBrowser.selectedBrowser.removeEventListener("load", checkResult, true);
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
    content.location = gCurrentTest.location;
  } else {
    gCheckETLD = false;
    gTestDesc = "#" + gCurrentTestIndex + " (" + gCurrentTest.name + " without eTLD in identity icon label)";
    if (!gForward)
      gTestDesc += " (second time)";
    content.location.reload(true);
  }
}

function checkResult() {
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

  executeSoon(nextTest);
}
