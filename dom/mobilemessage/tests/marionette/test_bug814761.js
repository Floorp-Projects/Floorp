/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

var manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

// Note: 378 chars and below is fine, but 379 and above will cause the issue.
// Sending first message works, but second one we get emulator callback but
// the actual SMS is never received, so script will timeout waiting for the
// onreceived event. Also note that a single larger message (i.e. 1600
// characters) works; so it is not a compounded send limit.
var fromNumber = "5551110000";
var msgLength = 379;
var msgText = new Array(msgLength + 1).join('a');

var pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function(result) {
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

  manager.onreceived = function onreceived(event) {
    log("Received 'onreceived' event.");
    manager.onreceived = null;

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
