/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

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
  is(message.deliveryStatus, "success");
  is(message.sender, sender);
  is(message.receiver, null);
  is(message.body, body);
  is(message.messageClass, "normal");
  ok(message.timestamp instanceof Date);
  // SMSC timestamp is in seconds.
  ok(Math.floor(message.timestamp.getTime() / 1000) >= Math.floor(now / 1000));

  cleanUp();
};

function cleanUp() {
  if (!completed) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  finish();
}
