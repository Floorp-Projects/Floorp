/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

function testInvalidIccObject(aIcc) {
  // Test access iccInfo.
  try {
    is(aIcc.iccInfo, null, "iccInfo: expect to get null");
  } catch(e) {
    ok(false, "access iccInfo should not get exception");
  }

  // Test access cardState.
  try {
    is(aIcc.cardState, null, "cardState: expect to get null");
  } catch(e) {
    ok(false, "access cardState should not get exception");
  }

  // Test STK related function.
  try {
    aIcc.sendStkResponse({}, {});
    ok(false, "sendStkResponse() should get exception");
  } catch(e) {}
  try {
    aIcc.sendStkMenuSelection(0, false);
    ok(false, "sendStkMenuSelection() should get exception");
  } catch(e) {}
  try {
    aIcc.sendStkTimerExpiration({});
    ok(false, "sendStkTimerExpiration() should get exception");
  } catch(e) {}
  try {
    aIcc.sendStkEventDownload({});
    ok(false, "sendStkEventDownload() should get exception");
  } catch(e) {}

  // Test card lock related function.
  try {
    aIcc.getCardLock("pin");
    ok(false, "getCardLock() should get exception");
  } catch(e) {}
  try {
    aIcc.unlockCardLock({});
    ok(false, "unlockCardLock() should get exception");
  } catch(e) {}
  try {
    aIcc.setCardLock({});
    ok(false, "setCardLock() should get exception");
  } catch(e) {}
  try {
    aIcc.getCardLockRetryCount("pin");
    ok(false, "getCardLockRetryCount() should get exception");
  } catch(e) {}

  // Test contact related function.
  try {
    aIcc.readContacts("adn");
    ok(false, "readContacts() should get exception");
  } catch(e) {}
  try {
    aIcc.updateContact("adn", {});
    ok(false, "updateContact() should get exception");
  } catch(e) {}

  // Test mvno function.
  try {
    aIcc.matchMvno("imsi");
    ok(false, "matchMvno() should get exception");
  } catch(e) {}

  // Test service state function.
  return aIcc.getServiceState("fdn").then(() => {
    ok(false, "getServiceState() should be rejected");
  }, () => {});
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  return Promise.resolve()
    // Turn off radio.
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(false));
      promises.push(waitForTargetEvent(iccManager, "iccundetected"));
      return Promise.all(promises);
    })
    // Test accessing invalid icc object.
    .then(() => testInvalidIccObject(icc))
    // We should restore the radio status.
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(true));
      promises.push(waitForTargetEvent(iccManager, "iccdetected"));
      return Promise.all(promises);
    });
});
