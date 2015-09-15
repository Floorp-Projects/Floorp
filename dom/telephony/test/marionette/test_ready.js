/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

var telephony = window.navigator.mozTelephony;
ok(telephony);

telephony.ready.then(function() {
  log("Telephony got ready");

  // Test telephony.ready in another window.
  let iframe = document.createElement("iframe");
  iframe.addEventListener("load", function load() {
    iframe.removeEventListener("load", load);

    let iframeTelephony = iframe.contentWindow.navigator.mozTelephony;
    ok(iframeTelephony);

    iframeTelephony.ready.then(function() {
      log("Telephony in iframe got ready");

      cleanUp();
    });
  });

  document.body.appendChild(iframe);
});
