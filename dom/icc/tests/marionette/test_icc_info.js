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

function setEmulatorMccMnc(mcc, mnc) {
  let cmd = "operator set 0 Android,Android," + mcc + mnc;
  emulatorHelper.sendCommand(cmd, function (result) {
    let re = new RegExp("" + mcc + mnc + "$");
    ok(result[0].match(re), "MCC/MNC should be changed.");
  });
}

/* Basic test */
taskHelper.push(function basicTest() {
  let iccInfo = icc.iccInfo;

  is(iccInfo.iccType, "sim");
  // The emulator's hard coded iccid value.
  // See it here {B2G_HOME}/external/qemu/telephony/sim_card.c#L299.
  is(iccInfo.iccid, 89014103211118510720);
  // The emulator's hard coded mcc and mnc codes.
  // See it here {B2G_HOME}/external/qemu/telephony/android_modem.c#L2465.
  is(iccInfo.mcc, 310);
  is(iccInfo.mnc, 260);
  is(iccInfo.spn, "Android");
  // Phone number is hardcoded in MSISDN
  // See {B2G_HOME}/external/qemu/telephony/sim_card.c, in asimcard_io()
  is(iccInfo.msisdn, "15555215554");

  taskHelper.runNext();
});

/* Test display condition change */
taskHelper.push(function testDisplayConditionChange() {
  function testSPN(mcc, mnc, expectedIsDisplayNetworkNameRequired,
                   expectedIsDisplaySpnRequired, callback) {
    icc.addEventListener("iccinfochange", function handler() {
      icc.removeEventListener("iccinfochange", handler);
      is(icc.iccInfo.isDisplayNetworkNameRequired,
         expectedIsDisplayNetworkNameRequired);
      is(icc.iccInfo.isDisplaySpnRequired,
         expectedIsDisplaySpnRequired);
      // operatorchange will be ignored if we send commands too soon.
      window.setTimeout(callback, 100);
    });
    // Send emulator command to change network mcc and mnc.
    setEmulatorMccMnc(mcc, mnc);
  }

  let testCases = [
    // [MCC, MNC, isDisplayNetworkNameRequired, isDisplaySpnRequired]
    [123, 456, false, true], // Not in HPLMN.
    [234, 136,  true, true], // Not in HPLMN, but in PLMN specified in SPDI.
    [123, 456, false, true], // Not in HPLMN. Triggering iccinfochange
    [466,  92,  true, true], // Not in HPLMN, but in another PLMN specified in SPDI.
    [123, 456, false, true], // Not in HPLMN. Triggering iccinfochange
    [310, 260,  true, true], // inside HPLMN.
  ];

  (function do_call(index) {
    let next = index < (testCases.length - 1) ? do_call.bind(null, index + 1) : taskHelper.runNext.bind(taskHelper);
    testCases[index].push(next);
    testSPN.apply(null, testCases[index]);
  })(0);
});

/* Test iccInfo when card becomes undetected */
taskHelper.push(function testCardIsNotReady() {
  // Turn off radio.
  setRadioEnabled(false);
  icc.addEventListener("iccinfochange", function oniccinfochange() {
    // Expect iccInfo changes to null
    if (icc.iccInfo === null) {
      icc.removeEventListener("iccinfochange", oniccinfochange);
      // We should restore radio status and expect to get iccdetected event.
      setRadioEnabled(true);
      iccManager.addEventListener("iccdetected", function oniccdetected(evt) {
        log("icc detected: " + evt.iccId);
        iccManager.removeEventListener("iccdetected", oniccdetected);
        taskHelper.runNext();
      });
    }
  });
});

// Start test
taskHelper.runNext();
