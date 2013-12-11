/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

function setRadioEnabled(enabled) {
  let connection = navigator.mozMobileConnections[0];
  ok(connection);

  let request  = connection.setRadioEnabled(enabled);

  request.onsuccess = function onsuccess() {
    log('setRadioEnabled: ' + enabled);
  };

  request.onerror = function onerror() {
    ok(false, "setRadioEnabled should be ok");
  };
}

/* Test access invalid icc object */
taskHelper.push(function testAccessRemovedIccObject() {
  setRadioEnabled(false);
  iccManager.addEventListener("iccundetected", function oniccundetected(evt) {
    log("got icc undetected event");
    iccManager.removeEventListener("iccundetected", oniccundetected);
    is(evt.iccId, iccId, "icc " + evt.iccId + " becomes undetected");

    // Test access iccInfo.
    try {
      is(icc.iccInfo, null, "iccInfo: expect to get null");
    } catch(e) {
      ok(false, "access iccInfo should not get exception");
    }

    // Test access cardState.
    try {
      is(icc.cardState, null, "cardState: expect to get null");
    } catch(e) {
      ok(false, "access cardState should not get exception");
    }

    // Test STK related function.
    try {
      icc.sendStkResponse({}, {});
      ok(false, "sendStkResponse() should get exception");
    } catch(e) {}
    try {
      icc.sendStkMenuSelection(0, false);
      ok(false, "sendStkMenuSelection() should get exception");
    } catch(e) {}
    try {
      icc.sendStkTimerExpiration({});
      ok(false, "sendStkTimerExpiration() should get exception");
    } catch(e) {}
    try {
      icc.sendStkEventDownload({});
      ok(false, "sendStkEventDownload() should get exception");
    } catch(e) {}

    // Test card lock related function.
    try {
      icc.getCardLock("");
      ok(false, "getCardLock() should get exception");
    } catch(e) {}
    try {
      icc.unlockCardLock({});
      ok(false, "unlockCardLock() should get exception");
    } catch(e) {}
    try {
      icc.setCardLock({});
      ok(false, "setCardLock() should get exception");
    } catch(e) {}
    try {
      icc.getCardLockRetryCount("");
      ok(false, "getCardLockRetryCount() should get exception");
    } catch(e) {}

    // Test contact related function.
    try {
      icc.readContacts("");
      ok(false, "readContacts() should get exception");
    } catch(e) {}
    try {
      icc.updateContact("", {});
      ok(false, "updateContact() should get exception");
    } catch(e) {}

    // Test secure element related function.
    try {
      icc.iccOpenChannel("");
      ok(false, "iccOpenChannel() should get exception");
    } catch(e) {}
    try {
      icc.iccExchangeAPDU(0, {});
      ok(false, "iccExchangeAPDU() should get exception");
    } catch(e) {}
    try {
      icc.iccCloseChannel(0);
      ok(false, "iccCloseChannel() should get exception");
    } catch(e) {}

    // We should restore the radio status.
    setRadioEnabled(true);
    iccManager.addEventListener("iccdetected", function oniccdetected(evt) {
      iccManager.removeEventListener("iccdetected", oniccdetected);
      taskHelper.runNext();
    });
  });
});

// Start test
taskHelper.runNext();
