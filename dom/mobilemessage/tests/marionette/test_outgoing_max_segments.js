/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let manager = window.navigator.mozMobileMessage;
// https://developer.mozilla.org/en-US/docs/DOM/SmsManager
let maxCharsPerSms = 160;
let maxSegments = 10; // 10 message segments concatenated into 1 multipart SMS

function verifyInitialState() {
  log("Verifying initial state.");
  ok(manager, "mozMobileMessage");
  sendSms();
}

function sendSms() {
  let destNumber = "5551234567";
  let msgText = "";
  let gotReqOnSuccess = false;
  let gotSmsOnSent = false;
  let sentSms;

  // Build the message text
  msgText = new Array((maxCharsPerSms * maxSegments) + 1).join('a');
  log("Sending multipart SMS (" + msgText.length + " chars total).");

  manager.onsent = function(event) {
    manager.onsent = null;
    log("Received 'onsent' event.");

    gotSmsOnSent = true;
    sentSms = event.message;
    ok(sentSms, "outgoing sms");
    ok(sentSms.id, "sms id");
    log("Sent SMS (id: " + sentSms.id + ").");
    ok(sentSms.threadId, "thread id");
    is(sentSms.body.length, msgText.length, "text length");
    is(sentSms.body, msgText, "msg body");
    is(sentSms.delivery, "sent", "delivery");

    if (gotReqOnSuccess) { verifySmsExists(sentSms); }
  };

  let requestRet = manager.send(destNumber, msgText);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    gotReqOnSuccess = true;
    if (event.target.result) {
      if (gotSmsOnSent) { verifySmsExists(sentSms); }
    } else {
      log("smsrequest returned false for manager.send");
      ok(false, "SMS send failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.send request returned unexpected error: " +
              event.target.error.name);
    cleanUp();
  };
}

function verifySmsExists(sentSms) {
  log("Getting SMS (id: " + sentSms.id + ").");
  let requestRet = manager.getMessage(sentSms.id);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, sentSms.id, "found SMS id matches");
    is(foundSms.threadId, sentSms.threadId, "found SMS thread id matches");
    is(foundSms.body.length, sentSms.body.length, "found SMS text length");
    is(foundSms.body, sentSms.body, "found SMS msg text matches");
    log("Got SMS (id: " + foundSms.id + ") as expected.");
    deleteSms(sentSms);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + sentSms.id + ") but should have.");
    ok(false, "SMS was not found");
    cleanUp();
  };
}

function deleteSms(smsMsgObj) {
  log("Deleting SMS (id: " + smsMsgObj.id + ") using smsmsg obj parameter.");
  let requestRet = manager.delete(smsMsgObj);
  ok(requestRet,"smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      cleanUp();
    } else {
      log("smsrequest returned false for manager.delete");
      ok(false, "SMS delete failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.delete request returned unexpected error: " +
              event.target.error.name);
    cleanUp();
  };
}

function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
