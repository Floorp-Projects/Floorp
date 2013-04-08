/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "5555552368"; // the remote number
const RECEIVER = "15555215554"; // the emulator's number

let sms = window.navigator.mozSms;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  simulateIncomingSms();  
}

function simulateIncomingSms() {
  let msgText = "";

  log("Simulating incoming SMS.");

  // Have message text > max SMS size (160 char) so will be a multi-part SMS
  for (var x = 1; x <= 24; x++) {
    msgText += 'FirefoxOS ';
  }

  sms.onreceived = function onreceived(event) {
    log("Received 'onreceived' smsmanager event.");
    let incomingSms = event.message;
    ok(incomingSms, "incoming sms");
    ok(incomingSms.id, "sms id");
    log("Received SMS (id: " + incomingSms.id + ").");
    ok(incomingSms.threadId, "thread id");
    is(incomingSms.body, msgText, "msg body");
    is(incomingSms.delivery, "received", "delivery");
    is(incomingSms.read, false, "read");
    is(incomingSms.receiver, RECEIVER, "receiver");
    is(incomingSms.sender, SENDER, "sender");
    ok(incomingSms.timestamp instanceof Date, "timestamp is istanceof date");

    verifySmsExists(incomingSms);
  };
  runEmulatorCmd("sms send " + SENDER + " " + msgText, function(result) {
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
    is(foundSms.threadId, incomingSms.threadId, "found SMS thread id matches");
    is(foundSms.body, incomingSms.body, "found SMS msg text matches");
    is(foundSms.delivery, "received", "delivery");
    is(foundSms.read, false, "read");
    is(foundSms.receiver, RECEIVER, "receiver");
    is(foundSms.sender, SENDER, "sender");
    log("Got SMS (id: " + foundSms.id + ") as expected.");
    deleteSms(incomingSms);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + incomingSms.id + ") but should have.");
    ok(false,"SMS was not found");
    cleanUp();
  };
}

function deleteSms(smsMsgObj){
  log("Deleting SMS (id: " + smsMsgObj.id + ") using smsmsg obj parameter.");
  let requestRet = sms.delete(smsMsgObj);
  ok(requestRet,"smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if(event.target.result){
      cleanUp();
    } else {
      log("smsrequest returned false for sms.delete");
      ok(false,"SMS delete failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.delete request returned unexpected error: "
        + event.target.error.name );
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
