/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 20000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let sms = window.navigator.mozSms;
// https://developer.mozilla.org/en-US/docs/DOM/SmsManager
let maxCharsPerSms = 160;
let maxSegments = 10; // 10 message segments concatenated into 1 multipart SMS

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  simulateIncomingSms();
}

function simulateIncomingSms() {
  let fromNumber = "5551234567";
  let msgText = "";

  // Build the message text
  msgText = new Array((maxCharsPerSms * maxSegments) + 1).join('a');
  log("Simulating incoming multipart SMS (" + msgText.length +
      " chars total).");

  sms.onreceived = function onreceived(event) {
    log("Received 'onreceived' smsmanager event.");
    let incomingSms = event.message;
    ok(incomingSms, "incoming sms");
    ok(incomingSms.id, "sms id");
    log("Received SMS (id: " + incomingSms.id + ").");
    is(incomingSms.body.length, msgText.length, "msg body length");
    is(incomingSms.body, msgText, "msg body");
    is(incomingSms.delivery, "received", "delivery");
    is(incomingSms.read, false, "read");
    is(incomingSms.receiver, null, "receiver");
    is(incomingSms.sender, fromNumber, "sender");
    ok(incomingSms.timestamp instanceof Date, "timestamp is instanceof date");

    verifySmsExists(incomingSms);
  };
  runEmulatorCmd("sms send " + fromNumber + " " + msgText, function(result) {
    is(result[0], "OK", "emulator output");
  });
}

function verifySmsExists(incomingSms) {
  log("Getting SMS (id: " + incomingSms.id + ").");
  let requestRet = sms.getMessage(incomingSms.id);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, incomingSms.id, "found SMS id matches");
    is(foundSms.body.length, incomingSms.body.length, "SMS text length");
    is(foundSms.body, incomingSms.body, "found SMS msg text matches");
    log("Got SMS (id: " + foundSms.id + ") as expected.");
    deleteSms(incomingSms);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + incomingSms.id + ") but should have.");
    ok(false, "SMS was not found");
    cleanUp();
  };
}

function deleteSms(smsMsgObj){
  log("Deleting SMS (id: " + smsMsgObj.id + ") using smsmsg obj parameter.");
  let requestRet = sms.delete(smsMsgObj);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      cleanUp();
    } else {
      log("smsrequest returned false for sms.delete");
      ok(false, "SMS delete failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.delete request returned unexpected error: " +
              event.target.error.name);
    cleanUp();
  };
}

function cleanUp() {
  sms.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
