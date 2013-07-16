/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let icc;
let iccInfo;
ifr.onload = function() {
  icc = ifr.contentWindow.navigator.mozIccManager;
  ok(icc instanceof ifr.contentWindow.MozIccManager,
     "icc is instanceof " + icc.constructor);

  iccInfo = icc.iccInfo;

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

  testDisplayConditionChange(testSPN, [
    // [MCC, MNC, isDisplayNetworkNameRequired, isDisplaySpnRequired]
    [123, 456, false, true], // Not in HPLMN.
    [234, 136,  true, true], // Not in HPLMN, but in PLMN specified in SPDI.
    [123, 456, false, true], // Not in HPLMN. Triggering iccinfochange
    [466,  92,  true, true], // Not in HPLMN, but in another PLMN specified in SPDI.
    [123, 456, false, true], // Not in HPLMN. Triggering iccinfochange
    [310, 260,  true, true], // inside HPLMN.
  ], finalize);
};
document.body.appendChild(ifr);

let emulatorCmdPendingCount = 0;
function sendEmulatorCommand(cmd, callback) {
  emulatorCmdPendingCount++;
  runEmulatorCmd(cmd, function (result) {
    emulatorCmdPendingCount--;
    is(result[result.length - 1], "OK");
    callback(result);
  });
}

function setEmulatorMccMnc(mcc, mnc) {
  let cmd = "operator set 0 Android,Android," + mcc + mnc;
  sendEmulatorCommand(cmd, function (result) {
    let re = new RegExp("" + mcc + mnc + "$");
    ok(result[0].match(re), "MCC/MNC should be changed.");
  });
}

function waitForIccInfoChange(callback) {
  icc.addEventListener("iccinfochange", function handler() {
    icc.removeEventListener("iccinfochange", handler);
    callback();
  });
}

function finalize() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

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
    is(iccInfo.isDisplayNetworkNameRequired,
       expectedIsDisplayNetworkNameRequired);
    is(iccInfo.isDisplaySpnRequired,
       expectedIsDisplaySpnRequired);
    // operatorchange will be ignored if we send commands too soon.
    window.setTimeout(callback, 100);
  });
  setEmulatorMccMnc(mcc, mnc);
}
