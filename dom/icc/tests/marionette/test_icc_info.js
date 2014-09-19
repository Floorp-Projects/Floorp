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
  emulatorHelper.sendCommand(cmd, function(result) {
    let re = new RegExp("" + mcc + mnc + "$");
    ok(result[0].match(re), "MCC/MNC should be changed.");
  });
}

/* Basic test */
taskHelper.push(function basicTest() {
  let iccInfo = icc.iccInfo;

  // The emulator's hard coded iccid value.
  // See it here {B2G_HOME}/external/qemu/telephony/sim_card.c#L299.
  is(iccInfo.iccid, 89014103211118510720);

  if (iccInfo instanceof MozGsmIccInfo) {
    log("Test Gsm IccInfo");
    is(iccInfo.iccType, "sim");
    is(iccInfo.spn, "Android");
    // The emulator's hard coded mcc and mnc codes.
    // See it here {B2G_HOME}/external/qemu/telephony/android_modem.c#L2465.
    is(iccInfo.mcc, 310);
    is(iccInfo.mnc, 260);
    // Phone number is hardcoded in MSISDN
    // See {B2G_HOME}/external/qemu/telephony/sim_card.c, in asimcard_io()
    is(iccInfo.msisdn, "15555215554");
  } else {
    log("Test Cdma IccInfo");
    is(iccInfo.iccType, "ruim");
    // MDN is hardcoded as "8587777777".
    // See it here {B2G_HOME}/hardware/ril/reference-ril/reference-ril.c,
    // in requestCdmaSubscription()
    is(iccInfo.mdn, "8587777777");
    // PRL version is hardcoded as 1.
    // See it here {B2G_HOME}/hardware/ril/reference-ril/reference-ril.c,
    // in requestCdmaSubscription()
    is(iccInfo.prlVersion, 1);
  }

  taskHelper.runNext();
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
