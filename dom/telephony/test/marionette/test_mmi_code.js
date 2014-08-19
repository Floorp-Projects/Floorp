/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "";

function dialMMI() {
  telephony.dial("*#06#").then(null, cause => {
    log("Received promise 'reject'");

    is(telephony.active, null);
    is(telephony.calls.length, 0);
    is(cause, "BadNumberError");

    finish();
  });
}

startTest(function() {
  dialMMI();
});
