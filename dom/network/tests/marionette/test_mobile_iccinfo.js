/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

let emulatorCmdPendingCount = 0;
function sendEmulatorCommand(cmd, callback) {
  emulatorCmdPendingCount++;
  runEmulatorCmd(cmd, function (result) {
    emulatorCmdPendingCount--;
    is(result[result.length - 1], "OK");
    callback(result);
  });
}

function setEmulatorMccMnc(mcc, mnc, callback) {
  let cmd = "operator set 0 Android,Android," + mcc + mnc;
  sendEmulatorCommand(cmd, function (result) {
    let re = new RegExp("" + mcc + mnc + "$");
    ok(result[0].match(re), "MCC/MNC should be changed.");
    if (callback) {
      callback();
    }
  });
}

function waitForIccInfoChange(callback) {
  connection.addEventListener("iccinfochange", function handler() {
    connection.removeEventListener("iccinfochange", handler);
    callback();
  });
}

function finalize() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

// The emulator's hard coded iccid value.
// See it here {B2G_HOME}/external/qemu/telephony/sim_card.c#L299.
is(connection.iccInfo.iccid, 89014103211118510720);

// The emulator's hard coded mcc and mnc codes.
// See it here {B2G_HOME}/external/qemu/telephony/android_modem.c#L2465.
is(connection.iccInfo.mcc, 310);
is(connection.iccInfo.mnc, 260);
is(connection.iccInfo.spn, "Android");
// Phone number is hardcoded in MSISDN
// See {B2G_HOME}/external/qemu/telephony/sim_card.c, in asimcard_io()
is(connection.iccInfo.msisdn, "15555215554");

// Test display condition change.
function testDisplayConditionChange(func, caseArray, oncomplete) {
  (function do_call(index) {
    let next = index < (caseArray.length - 1) ? do_call.bind(null, index + 1) : oncomplete;
    caseArray[index].push(next);
    func.apply(null, caseArray[index]);
  })(0);
}

function testSPN(mcc, mnc, expectedIsDisplayNetworkNameRequired,
                  expectedIsDisplaySpnRequired, callback) {
  waitForIccInfoChange(function() {
    is(connection.iccInfo.isDisplayNetworkNameRequired,
       expectedIsDisplayNetworkNameRequired);
    is(connection.iccInfo.isDisplaySpnRequired,
       expectedIsDisplaySpnRequired);
    window.setTimeout(callback, 0);
  });
  setEmulatorMccMnc(mcc, mnc);
}

testDisplayConditionChange(testSPN, [
  [123, 456, false, true], // Not in HPLMN.
  [310, 260, true, true], // inside HPLMN.
], finalize);
