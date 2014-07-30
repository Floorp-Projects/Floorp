/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

let telephony = window.navigator.mozTelephony;
ok(telephony);

telephony.onready = function() {
  log("Receive 'ready' event");

  // Test registering 'ready' event in another window.
  let iframe = document.createElement("iframe");
  iframe.addEventListener("load", function load() {
    iframe.removeEventListener("load", load);

    let iframeTelephony = iframe.contentWindow.navigator.mozTelephony;
    ok(iframeTelephony);

    iframeTelephony.onready = function() {
      log("Receive 'ready' event in iframe");

      cleanUp();
    };
  });

  document.body.appendChild(iframe);
};
