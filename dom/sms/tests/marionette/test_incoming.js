/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

const WHITELIST_PREF = "dom.sms.whitelist";
let uriPrePath = window.location.protocol + "//" + window.location.host;
SpecialPowers.setCharPref(WHITELIST_PREF, uriPrePath);

let sms = window.navigator.mozSms;
let sender = "5555552368";
let body = "Hello SMS world!";
let now = Date.now();

let completed = false;
runEmulatorCmd("sms send " + sender + " " + body, function(result) {
  log("Sent fake SMS: " + result);
  is(result[0], "OK");
  completed = true;
});

sms.onreceived = function onreceived(event) {
  log("Received an SMS!");

  let message = event.message;
  ok(message instanceof MozSmsMessage);

  is(message.delivery, "received");
  is(message.sender, sender);
  is(message.receiver, null);
  is(message.body, body);
  ok(message.timestamp instanceof Date);
  ok(message.timestamp.getTime() > now);

  cleanUp();
};

function cleanUp() {
  if (!completed) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}
