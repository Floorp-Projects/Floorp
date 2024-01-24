/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CPS = Cc["@mozilla.org/network/captive-portal-service;1"].getService(
  Ci.nsICaptivePortalService
);

async function checkCaptivePortalTabReference(evt, currState) {
  await portalDetected();
  let errorTab = await openCaptivePortalErrorTab();
  let portalTab = await openCaptivePortalLoginTab(errorTab);

  // Release the reference held to the portal tab by sending success/abort events.
  Services.obs.notifyObservers(null, evt);
  await TestUtils.waitForCondition(
    () => CPS.state == currState,
    "Captive portal has been released"
  );
  gBrowser.removeTab(errorTab);

  await portalDetected();
  Assert.equal(CPS.state, CPS.LOCKED_PORTAL, "Captive portal is locked again");
  errorTab = await openCaptivePortalErrorTab();
  let portalTab2 = await openCaptivePortalLoginTab(errorTab);
  Assert.notEqual(
    portalTab,
    portalTab2,
    "waitForNewTab in openCaptivePortalLoginTab should not have completed at this point if references were held to the old captive portal tab after login/abort."
  );
  gBrowser.removeTab(portalTab);
  gBrowser.removeTab(portalTab2);

  let errorTabReloaded = BrowserTestUtils.waitForErrorPage(
    errorTab.linkedBrowser
  );
  Services.obs.notifyObservers(null, "captive-portal-login-success");
  await errorTabReloaded;

  gBrowser.removeTab(errorTab);
}

add_setup(async function () {
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
