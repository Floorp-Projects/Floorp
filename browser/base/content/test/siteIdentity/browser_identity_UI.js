/* Tests for correct behaviour of getHostForDisplay on identity handler */

requestLongerTimeout(2);

// Greek IDN for 'example.test'.
var idnDomain =
  "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";
var tests = [
  {
    name: "normal domain",
    location: "http://test1.example.org/",
    hostForDisplay: "test1.example.org",
  },
  {
    name: "view-source",
    location: "view-source:http://example.com/",
    hostForDisplay: null,
  },
  {
    name: "normal HTTPS",
    location: "https://example.com/",
    hostForDisplay: "example.com",
  },
  {
    name: "IDN subdomain",
    location: "http://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/",
    hostForDisplay: "sub1." + idnDomain,
  },
  {
    name: "subdomain with port",
    location: "http://sub1.test1.example.org:8000/",
    hostForDisplay: "sub1.test1.example.org",
  },
  {
    name: "subdomain HTTPS",
    location: "https://test1.example.com/",
    hostForDisplay: "test1.example.com",
  },
  {
    name: "view-source HTTPS",
    location: "view-source:https://example.com/",
    hostForDisplay: null,
  },
  {
    name: "IP address",
    location: "http://127.0.0.1:8888/",
    hostForDisplay: "127.0.0.1",
  },
  {
    name: "about:certificate",
    location:
      "about:certificate?cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO&cert=1pVzllk7ZFHzANBgkqhkiG9w0BAQ",
    hostForDisplay: "about:certificate",
  },
  {
    name: "about:reader",
    location: "about:reader?url=http://example.com",
    hostForDisplay: "example.com",
  },
];

add_task(async function test() {
  ok(gIdentityHandler, "gIdentityHandler should exist");

  await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let i = 0; i < tests.length; i++) {
    await runTest(i, true);
  }

  gBrowser.removeCurrentTab();
  await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let i = tests.length - 1; i >= 0; i--) {
    await runTest(i, false);
  }

  gBrowser.removeCurrentTab();
});

async function runTest(i, forward) {
  let currentTest = tests[i];
  let testDesc = "#" + i + " (" + currentTest.name + ")";
  if (!forward) {
    testDesc += " (second time)";
  }

  info("Running test " + testDesc);

  let popupHidden = null;
  if ((forward && i > 0) || (!forward && i < tests.length - 1)) {
    popupHidden = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popuphidden"
    );
  }

  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    currentTest.location
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, currentTest.location);
  await loaded;
  await popupHidden;
  ok(
    BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
    "Control Center is hidden"
  );

  // Sanity check other values, and the value of gIdentityHandler.getHostForDisplay()
  is(
    gIdentityHandler._uri.spec,
    currentTest.location,
    "location matches for test " + testDesc
  );
  // getHostForDisplay can't be called for all modes
  if (currentTest.hostForDisplay !== null) {
    is(
      gIdentityHandler.getHostForDisplay(),
      currentTest.hostForDisplay,
      "hostForDisplay matches for test " + testDesc
    );
  }

  // Open the Control Center and make sure it closes after nav (Bug 1207542).
  let popupShown = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "popupshown"
  );
  gIdentityHandler._identityBox.click();
  info("Waiting for the Control Center to be shown");
  await popupShown;
  ok(
    !BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
    "Control Center is visible"
  );
  let displayedHost = currentTest.hostForDisplay || currentTest.location;
  ok(
    gIdentityHandler._identityPopupMainViewHeaderLabel.textContent.includes(
      displayedHost
    ),
    "identity UI header shows the host for test " + testDesc
  );

  // Show the subview, which is an easy way in automation to reproduce
  // Bug 1207542, where the CC wouldn't close on navigation.
  let promiseViewShown = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "ViewShown"
  );
  gBrowser.ownerDocument
    .querySelector("#identity-popup-security-expander")
    .click();
  await promiseViewShown;
}
