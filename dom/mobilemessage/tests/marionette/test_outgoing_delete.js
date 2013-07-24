/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.setBoolPref("dom.sms.requestStatusReport", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "15555215554"; // the emulator's number
const RECEIVER = "5551117777"; // the destination number

let sms = window.navigator.mozSms;
let msgText = "Mozilla Firefox OS!";
let gotSmsOnsent = false;
let gotReqOnsuccess = false;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  sendSms();
}

function sendSms() {
  let smsId = 0;

  log("Sending an SMS.");
  sms.onsent = function(event) {
    log("Received 'onsent' smsmanager event.");
    gotSmsOnsent = true;
    let sentSms = event.message;
    ok(sentSms, "outgoing sms");
    ok(sentSms.id, "sms id");
    smsId = sentSms.id;
    log("Sent SMS (id: " + smsId + ").");
    ok(sentSms.threadId, "thread id");
    is(sentSms.body, msgText, "msg body");
    is(sentSms.delivery, "sent", "delivery");
    is(sentSms.deliveryStatus, "pending", "deliveryStatus");
    is(sentSms.read, true, "read");
    is(sentSms.receiver, RECEIVER, "receiver");
    is(sentSms.sender, SENDER, "sender");
    is(sentSms.messageClass, "normal", "messageClass");
    ok(sentSms.timestamp instanceof Date, "timestamp is istanceof date");

    if (gotSmsOnsent && gotReqOnsuccess) { verifySmsExists(smsId); }
  };

  let requestRet = sms.send(RECEIVER, msgText);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    gotReqOnsuccess = true;
    if(event.target.result){
      if (gotSmsOnsent && gotReqOnsuccess) { verifySmsExists(smsId); }
    } else {
      log("smsrequest returned false for sms.send");
      ok(false,"SMS send failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.send request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function verifySmsExists(smsId) {
  log("Getting SMS (id: " + smsId + ").");
  let requestRet = sms.getMessage(smsId);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, smsId, "found SMS id matches");
    is(foundSms.body, msgText, "found SMS msg text matches");
    is(foundSms.delivery, "sent", "delivery");
    is(foundSms.read, true, "read");
    is(foundSms.receiver, RECEIVER, "receiver");
    is(foundSms.sender, SENDER, "sender");
    is(foundSms.messageClass, "normal", "messageClass");
    log("Got SMS (id: " + foundSms.id + ") as expected.");
    deleteSms(smsId);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + smsId + ") but should have.");
    ok(false,"SMS was not found");
    cleanUp();
  };
}

function deleteSms(smsId){
  log("Deleting SMS (id: " + smsId + ") using sms id parameter.");
  let requestRet = sms.delete(smsId);
  ok(requestRet,"smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if(event.target.result){
      verifySmsDeleted(smsId);
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

function verifySmsDeleted(smsId) {
  log("Getting SMS (id: " + smsId + ").");
  let requestRet = sms.getMessage(smsId);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, smsId, "found SMS id matches");
    is(foundSms.body, msgText, "found SMS msg text matches");
    log("Got SMS (id: " + foundSms.id + ") but should not have.");
    ok(false, "SMS was not deleted");
    cleanUp();
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + smsId + ") as expected.");
    cleanUp();
  };
}

function cleanUp() {
  sms.onsent = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.requestStatusReport");
  finish();
}

// Start the test
verifyInitialState();
