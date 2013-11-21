/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

function setRadioEnabled(enabled) {
  SpecialPowers.addPermission("settings-write", true, document);

  // TODO: Bug 856553 - [B2G] RIL: need an API to enable/disable radio
  let settings = navigator.mozSettings;
  let setLock = settings.createLock();
  let obj = {
    "ril.radio.disabled": !enabled
  };
  let setReq = setLock.set(obj);

  setReq.addEventListener("success", function onSetSuccess() {
    log("set 'ril.radio.disabled' to " + enabled);
  });

  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set 'ril.radio.disabled' to " + enabled);
  });

  SpecialPowers.removePermission("settings-write", document);
}

/* Test iccundetected event */
taskHelper.push(function testIccUndetectedEvent() {
  setRadioEnabled(false);
  iccManager.addEventListener("iccundetected", function oniccundetected(evt) {
    log("got icc undetected event");
    iccManager.removeEventListener("iccundetected", oniccundetected);

    // TODO: Bug 932650 - B2G RIL: WebIccManager API - add marionette tests for
    //                    multi-sim
    // In single sim scenario, there is only one sim card, we can use below way
    // to check iccIds.
    is(evt.iccId, iccId, "icc " + evt.iccId + " becomes undetected");
    is(iccManager.iccIds.length, 0,
       "iccIds.length becomes to " + iccManager.iccIds.length);
    is(iccManager.getIccById(evt.iccId), null,
       "should not get a valid icc object here");

    taskHelper.runNext();
  });
});

/* Test iccdetected event */
taskHelper.push(function testIccDetectedEvent() {
  setRadioEnabled(true);
  iccManager.addEventListener("iccdetected", function oniccdetected(evt) {
    log("got icc detected event");
    iccManager.removeEventListener("iccdetected", oniccdetected);

    // TODO: Bug 932650 - B2G RIL: WebIccManager API - add marionette tests for
    //                    multi-sim
    // In single sim scenario, there is only one sim card, we can use below way
    // to check iccIds.
    is(evt.iccId, iccId, "icc " + evt.iccId + " is detected");
    is(iccManager.iccIds.length, 1,
       "iccIds.length becomes to " + iccManager.iccIds.length);
    ok(iccManager.getIccById(evt.iccId) instanceof MozIcc,
       "should get a valid icc object here");

    taskHelper.runNext();
  });
});

// Start test
taskHelper.runNext();
