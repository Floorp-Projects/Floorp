/* Tests for correct behaviour of getHostForDisplay on identity handler */

requestLongerTimeout(2);

// Greek IDN for 'example.test'.
var idnDomain =
  "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";
var tests = [
  {
    name: "normal domain",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    location: "http://test1.example.org/",
    hostForDisplay: "test1.example.org",
    hasSubview: true,
  },
  {
    name: "view-source",
    location: "view-source:http://example.com/",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    newURI: "http://example.com/",
    hostForDisplay: "example.com",
    hasSubview: true,
  },
  {
    name: "normal HTTPS",
    location: "https://example.com/",
    hostForDisplay: "example.com",
    hasSubview: true,
  },
  {
    name: "IDN subdomain",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    location: "http://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/",
    hostForDisplay: "sub1." + idnDomain,
    hasSubview: true,
  },
  {
    name: "subdomain with port",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    location: "http://sub1.test1.example.org:8000/",
    hostForDisplay: "sub1.test1.example.org",
    hasSubview: true,
  },
  {
    name: "subdomain HTTPS",
    location: "https://test1.example.com/",
    hostForDisplay: "test1.example.com",
    hasSubview: true,
  },
  {
    name: "view-source HTTPS",
    location: "view-source:https://example.com/",
    newURI: "https://example.com/",
    hostForDisplay: "example.com",
    hasSubview: true,
  },
  {
    name: "IP address",
    location: "http://127.0.0.1:8888/",
    hostForDisplay: "127.0.0.1",
    hasSubview: false,
  },
  {
    name: "about:certificate",
    location:
      "about:certificate?cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO&cert=1pVzllk7ZFHzANBgkqhkiG9w0BAQ",
    hostForDisplay: "about:certificate",
    hasSubview: false,
  },
  {
    name: "about:reader",
    location: "about:reader?url=http://example.com",
    hostForDisplay: "example.com",
    hasSubview: false,
  },
  {
    name: "chrome:",
    location: "chrome://global/skin/in-content/info-pages.css",
    hostForDisplay: "chrome://global/skin/in-content/info-pages.css",
    hasSubview: false,
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
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    currentTest.location
  );
  await loaded;
  await popupHidden;
  ok(
    !gIdentityHandler._identityPopup ||
      BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
    "Control Center is hidden"
  );

  // Sanity check other values, and the value of gIdentityHandler.getHostForDisplay()
  is(
    gIdentityHandler._uri.spec,
    currentTest.newURI || currentTest.location,
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
    window,
    "popupshown",
    true,
    event => event.target == gIdentityHandler._identityPopup
  );
  gIdentityHandler._identityIconBox.click();
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

  let securityButton = gBrowser.ownerDocument.querySelector(
    "#identity-popup-security-button"
  );
  is(
    securityButton.disabled,
    !currentTest.hasSubview,
    "Security button has correct disabled state"
  );
  if (currentTest.hasSubview) {
    // Show the subview, which is an easy way in automation to reproduce
    // Bug 1207542, where the CC wouldn't close on navigation.
    let promiseViewShown = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "ViewShown"
    );
    securityButton.click();
    await promiseViewShown;
  }
}
