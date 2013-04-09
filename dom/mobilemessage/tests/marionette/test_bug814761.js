/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let sms = window.navigator.mozSms;

// Note: 378 chars and below is fine, but 379 and above will cause the issue.
// Sending first message works, but second one we get emulator callback but
// the actual SMS is never received, so script will timeout waiting for the
// sms.onreceived event. Also note that a single larger message (i.e. 1600
// characters) works; so it is not a compounded send limit.
let fromNumber = "5551110000";
let msgLength = 379;
let msgText = new Array(msgLength + 1).join('a');

let pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");
  });
}

function firstIncomingSms() {
  simulateIncomingSms(secondIncomingSms);
}

function secondIncomingSms() {
  simulateIncomingSms(cleanUp);
}

function simulateIncomingSms(nextFunction) {
  log("Simulating incoming multipart SMS (" + msgText.length
      + " chars total).");

  sms.onreceived = function onreceived(event) {
    log("Received 'onreceived' smsmanager event.");
    sms.onreceived = null;

    let incomingSms = event.message;
    ok(incomingSms, "incoming sms");
    is(incomingSms.body, msgText, "msg body");

    window.setTimeout(nextFunction, 0);
  };

  sendSmsToEmulator(fromNumber, msgText);
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
firstIncomingSms();
