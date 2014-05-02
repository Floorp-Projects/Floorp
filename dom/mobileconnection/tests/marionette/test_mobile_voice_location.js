/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function verifyVoiceCellLocationInfo(aLac, aCid) {
  let cell = mobileConnection.voice.cell;
  ok(cell, "location available");

  is(cell.gsmLocationAreaCode, aLac, "check voice.cell.gsmLocationAreaCode");
  is(cell.gsmCellId, aCid, "check voice.cell.gsmCellId");
  is(cell.cdmaBaseStationId, -1, "check voice.cell.cdmaBaseStationId");
  is(cell.cdmaBaseStationLatitude, -2147483648,
     "check voice.cell.cdmaBaseStationLatitude");
  is(cell.cdmaBaseStationLongitude, -2147483648,
     "check voice.cell.cdmaBaseStationLongitude");
  is(cell.cdmaSystemId, -1, "check voice.cell.cdmaSystemId");
  is(cell.cdmaNetworkId, -1, "check voice.cell.cdmaNetworkId");
}

/* Test Voice Cell Location Info Change */
function testVoiceCellLocationUpdate(aLac, aCid) {
  // Set emulator's lac/cid and wait for 'onvoicechange' event.
  log("Test cell location with lac=" + aLac + " and cid=" + aCid);

  return setEmulatorGsmLocationAndWait(aLac, aCid, true, false)
    .then(() => verifyVoiceCellLocationInfo(aLac, aCid));
}

startTestCommon(function() {
  return getEmulatorGsmLocation()
    .then(function(aResult) {
      log("Test initial voice location info");
      verifyVoiceCellLocationInfo(aResult.lac, aResult.cid);

      return Promise.resolve()
        .then(() => testVoiceCellLocationUpdate(100, 100))
        .then(() => testVoiceCellLocationUpdate(2000, 2000))

        // Reset back to initial values.
        .then(() => testVoiceCellLocationUpdate(aResult.lac, aResult.cid));
    });
});
