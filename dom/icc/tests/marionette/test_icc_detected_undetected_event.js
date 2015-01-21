/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

// Start tests
startTestCommon(function() {
  let origNumIccs = iccManager.iccIds.length;
  let icc = getMozIcc();
  let iccId = icc.iccInfo.iccid;
  let mobileConnection = getMozMobileConnectionByServiceId();

  return Promise.resolve()
    // Test iccundetected event.
    .then(() => {
      let promises = [];

      promises.push(setRadioEnabled(false));
      promises.push(waitForTargetEvent(iccManager, "iccundetected").then((aEvt) => {
        is(aEvt.iccId, iccId, "icc " + aEvt.iccId + " becomes undetected");
        is(iccManager.iccIds.length, origNumIccs - 1,
           "iccIds.length becomes to " + iccManager.iccIds.length);
        is(iccManager.getIccById(aEvt.iccId), null,
           "should not get a valid icc object here");

        // The mozMobileConnection.iccId should be in sync.
        is(mobileConnection.iccId, null, "check mozMobileConnection.iccId");
      }));

      return Promise.all(promises);
    })
    // Test iccdetected event.
    .then(() => {
      let promises = [];

      promises.push(setRadioEnabled(true));
      promises.push(waitForTargetEvent(iccManager, "iccdetected").then((aEvt) => {
        is(aEvt.iccId, iccId, "icc " + aEvt.iccId + " is detected");
        is(iccManager.iccIds.length, origNumIccs,
           "iccIds.length becomes to " + iccManager.iccIds.length);
        ok(iccManager.getIccById(aEvt.iccId) instanceof MozIcc,
           "should get a valid icc object here");

        // The mozMobileConnection.iccId should be in sync.
        is(mobileConnection.iccId, iccId, "check mozMobileConnection.iccId");
      }));

      return Promise.all(promises);
    });
});
