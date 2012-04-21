/* Tests for correct behaviour of getEffectiveHost on identity handler */
function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.identity.ssl_domain_display");
  });

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
    host: "test1.example.org",
    effectiveHost: "example.org"
  },
  {
    name: "view-source",
    location: "view-source:http://example.com/",
    // TODO: these should not be blank, bug 646690
    host: "",
    effectiveHost: ""
  },
  {
    name: "normal HTTPS",
    location: "https://example.com/",
    host: "example.com",
    effectiveHost: "example.com",
    isHTTPS: true
  },
  {
    name: "IDN subdomain",
    location: "http://sub1." + idnDomain + "/",
    host: "sub1." + idnDomain,
    effectiveHost: idnDomain
  },
  {
    name: "subdomain with port",
    location: "http://sub1.test1.example.org:8000/",
    host: "sub1.test1.example.org:8000",
    effectiveHost: "example.org"
  },
  {
    name: "subdomain HTTPS",
    location: "https://test1.example.com",
    host: "test1.example.com",
    effectiveHost: "example.com",
    isHTTPS: true
  },
  {
    name: "view-source HTTPS",
    location: "view-source:https://example.com/",
    // TODO: these should not be blank, bug 646690
    host: "",
    effectiveHost: "",
    isHTTPS: true
  },
  {
    name: "IP address",
    location: "http://127.0.0.1:8888/",
    host: "127.0.0.1:8888",
    effectiveHost: "127.0.0.1"
  },
]

let gCurrentTest, gCurrentTestIndex = -1, gTestDesc;
// Go through the tests in both directions, to add additional coverage for
// transitions between different states.
let gForward = true;
let gCheckETLD = false;
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
      Services.prefs.setIntPref("browser.identity.ssl_domain_display", 1);
    }
    content.location = gCurrentTest.location;
  } else {
    gCheckETLD = false;
    gTestDesc = "#" + gCurrentTestIndex + " (" + gCurrentTest.name + " without eTLD in identity icon label)";
    if (!gForward)
      gTestDesc += " (second time)";
    Services.prefs.clearUserPref("browser.identity.ssl_domain_display");
    content.location.reload(true);
  }
}

function checkResult() {
  if (gCurrentTest.isHTTPS && Services.prefs.getIntPref("browser.identity.ssl_domain_display") == 1) {
    // Check that the effective host is displayed in the UI
    let label = document.getElementById("identity-icon-label");
    is(label.value, gCurrentTest.effectiveHost, "effective host is displayed in identity icon label for test " + gTestDesc);
  }

  // Sanity check other values, and the value of gIdentityHandler.getEffectiveHost()
  is(gIdentityHandler._lastLocation.host, gCurrentTest.host, "host matches for test " + gTestDesc);
  is(gIdentityHandler.getEffectiveHost(), gCurrentTest.effectiveHost, "effectiveHost matches for test " + gTestDesc);

  executeSoon(nextTest);
}
