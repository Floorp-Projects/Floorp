/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gServer;
let gServerURL;
let gRedirectedURL;
let gExpectedText;
let gNavigated = false;
let gExpectedURL;

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

let gCaptivePortalState = "captive-replace";

add_setup(async function () {
  gOverride.addIPOverride("test1.example.com", "127.0.0.1");
  gOverride.addIPOverride("example.net", "127.0.0.1");
  gServer = new HttpServer();
  gServer.start(-1);
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  gServerURL = `http://test1.example.com:${gServer.identity.primaryPort}/`;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  gRedirectedURL = `http://example.net:${gServer.identity.primaryPort}/redir`;
  gServer.identity.add(
    "http",
    "test1.example.com",
    gServer.identity.primaryPort
  );
  gServer.identity.add("http", "example.net", gServer.identity.primaryPort);
  gServer.registerPathHandler("/", (request, response) => {
    if (gCaptivePortalState == "captive-replace") {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html");
      const BODY = `captive muahahah`;
      response.bodyOutputStream.write(BODY, BODY.length);
      return;
    }

    if (gCaptivePortalState == "captive-redirect") {
      response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      response.setHeader("Location", gRedirectedURL);
      response.write("redirecting");
    }
  });
  gServer.registerPathHandler("/redir", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html");
    const BODY = `redirected`;
    response.bodyOutputStream.write(BODY, BODY.length);
  });
  registerCleanupFunction(async () => {
    await gServer.stop();
    gOverride.clearOverrides();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["captivedetect.canonicalURL", gServerURL],
      ["captivedetect.canonicalContent", "stuff"],
      ["network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY],
      ["network.proxy.no_proxies_on", "test1.example.com,example.net"],
    ],
  });
});

async function openAndCheck() {
  // Open a second window in the background. Later, we'll check that
  // when we click the button to open the captive portal tab, the tab
  // only opens in the active window and not in the background one.
  let secondWindow = await openWindowAndWaitForFocus();
  await SimpleTest.promiseFocus(window);

  await portalDetected();

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  let tab = await openCaptivePortalErrorTab();
  let browser = tab.linkedBrowser;
  let portalTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, gExpectedURL);

  await SpecialPowers.spawn(browser, [], async () => {
    let doc = content.document;
    let loginButton = doc.getElementById("openPortalLoginPageButton");
    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.isVisible(loginButton),
      "Captive portal error page UI is visible"
    );

    if (!Services.focus.focusedElement == loginButton) {
      await ContentTaskUtils.waitForEvent(loginButton, "focus");
    }

    Assert.ok(true, "openPortalLoginPageButton has focus");
    info("Clicking the Open Login Page button");
    await EventUtils.synthesizeMouseAtCenter(loginButton, {}, content);
  });

  let portalTab = await portalTabPromise;
  await SpecialPowers.spawn(
    portalTab.linkedBrowser,
    [gExpectedText],
    async text => {
      Assert.equal(content.document.body.innerText, text);
    }
  );

  is(
    gBrowser.selectedTab,
    portalTab,
    "Login page should be open in a new foreground tab."
  );

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  // Make sure clicking the "Open Login Page" button again focuses the existing portal tab.
  await BrowserTestUtils.switchTab(gBrowser, tab);
  // Passing an empty function to BrowserTestUtils.switchTab lets us wait for an arbitrary
  // tab switch.
  portalTabPromise = BrowserTestUtils.switchTab(gBrowser, () => {});
  await SpecialPowers.spawn(browser, [], async () => {
    info("Clicking the Open Login Page button.");
    let loginButton = content.document.getElementById(
      "openPortalLoginPageButton"
    );
    await EventUtils.synthesizeMouseAtCenter(loginButton, {}, content);
  });

  info("Opening captive portal login page");
  let portalTab2 = await portalTabPromise;
  is(portalTab2, portalTab, "The existing portal tab should be focused.");

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  let portalTabClosing = BrowserTestUtils.waitForTabClosing(portalTab);
  let errorTabReloaded = BrowserTestUtils.waitForErrorPage(browser);

  Services.obs.notifyObservers(null, "captive-portal-login-success");
  info("closing page");
  if (gNavigated) {
    // Since it navigated, the tab won't be automatically closed.
    await BrowserTestUtils.removeTab(portalTab);
  }

  await portalTabClosing;

  info(
    "Waiting for error tab to be reloaded after the captive portal was freed."
  );
  await errorTabReloaded;
  await SpecialPowers.spawn(browser, [], () => {
    let doc = content.document;
    ok(
      !doc.body.classList.contains("captiveportal"),
      "Captive portal error page UI is not visible."
    );
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(secondWindow);
}

add_task(async function checkNoTRRForTab() {
  info("Check that the captive portal login tab does not use TRR");
  gExpectedText = `captive muahahah`;
  gExpectedURL = gServerURL;

  await openAndCheck();
});

add_task(async function checkNoTRRForTabWithRedirect() {
  gCaptivePortalState = "captive-redirect";
  gExpectedText = "redirected";
  gExpectedURL = gRedirectedURL;
  gNavigated = true;
  info("Check that the captive portal login tab does not use TRR");

  await openAndCheck();
});
