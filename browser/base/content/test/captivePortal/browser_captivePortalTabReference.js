/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BAD_CERT_PAGE = "https://expired.example.com/";
const CPS = Cc["@mozilla.org/network/captive-portal-service;1"].getService(
  Ci.nsICaptivePortalService
);

async function setCaptivePortalLoginState() {
  let captivePortalStatePropagated = TestUtils.topicObserved(
    "ipc:network:captive-portal-set-state"
  );
  Services.obs.notifyObservers(null, "captive-portal-login");
  info(
    "Waiting for captive portal login state to propagate to the content process."
  );
  await captivePortalStatePropagated;
  info("Captive Portal login state has been set");
}

async function openCaptivePortalErrorTab() {
  // Open a page with a cert error.
  let certErrorLoaded;
  let errorTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      let tab = BrowserTestUtils.addTab(gBrowser, BAD_CERT_PAGE);
      gBrowser.selectedTab = tab;
      let browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
      return tab;
    },
    false
  );

  await certErrorLoaded;
  info("An error page was opened");
  let browser = errorTab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let doc = content.document;
    let loginButton = doc.getElementById("openPortalLoginPageButton");
    await ContentTaskUtils.waitForCondition(
      () => loginButton && doc.body.className == "captiveportal",
      "Captive portal error page UI is visible"
    );
  });

  return errorTab;
}

async function openCaptivePortalLoginTab(errorTab) {
  let portalTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    CANONICAL_URL
  );

  await SpecialPowers.spawn(errorTab.linkedBrowser, [], async () => {
    let doc = content.document;
    let loginButton = doc.getElementById("openPortalLoginPageButton");
    info("Click on the login button on the captive portal error page");
    await EventUtils.synthesizeMouseAtCenter(loginButton, {}, content);
  });

  let portalTab = await portalTabPromise;
  is(
    gBrowser.selectedTab,
    portalTab,
    "Captive Portal login page is now open in a new foreground tab."
  );

  return portalTab;
}

async function checkCaptivePortalTabReference(evt, currState) {
  await setCaptivePortalLoginState();
  let errorTab = await openCaptivePortalErrorTab();
  let portalTab = await openCaptivePortalLoginTab(errorTab);

  // Release the reference held to the portal tab by sending success/abort events.
  Services.obs.notifyObservers(null, evt);
  await TestUtils.waitForCondition(
    () => CPS.state == currState,
    "Captive portal has been released"
  );
  gBrowser.removeTab(errorTab);

  await setCaptivePortalLoginState();
  ok(CPS.state == CPS.LOCKED_PORTAL, "Captive portal is locked again");
  errorTab = await openCaptivePortalErrorTab();
  let portalTab2 = await openCaptivePortalLoginTab(errorTab);
  ok(
    portalTab != portalTab2,
    "waitForNewTab in openCaptivePortalLoginTab should not have completed at this point if references were held to the old captive portal tab after login/abort."
  );

  gBrowser.removeTab(errorTab);
  gBrowser.removeTab(portalTab);
  gBrowser.removeTab(portalTab2);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["captivedetect.canonicalURL", CANONICAL_URL],
      ["captivedetect.canonicalContent", CANONICAL_CONTENT],
    ],
  });
});

let capPortalStates = [
  {
    evt: "captive-portal-login-success",
    state: CPS.UNLOCKED_PORTAL,
  },
  {
    evt: "captive-portal-login-abort",
    state: CPS.UNKNOWN,
  },
];

for (let elem of capPortalStates) {
  add_task(checkCaptivePortalTabReference.bind(null, elem.evt, elem.state));
}
