/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

function setRadioEnabled(enabled) {
  let connection = navigator.mozMobileConnections[0];
  ok(connection);

  let request  = connection.setRadioEnabled(enabled);

  request.onsuccess = function onsuccess() {
    log('setRadioEnabled: ' + enabled);
  };

  request.onerror = function onerror() {
    ok(false, "setRadioEnabled should be ok");
  };
}

/* Basic test */
taskHelper.push(function basicTest() {
  is(icc.cardState, "ready", "card state is " + icc.cardState);
  taskHelper.runNext();
});

/* Test cardstatechange event by switching radio off */
taskHelper.push(function testCardStateChange() {
  // Turn off radio.
  setRadioEnabled(false);
  icc.addEventListener("cardstatechange", function oncardstatechange() {
    log("card state changes to " + icc.cardState);
    // Expect to get card state changing to null.
    if (icc.cardState === null) {
      icc.removeEventListener("cardstatechange", oncardstatechange);
      // We should restore radio status and expect to get iccdetected event.
      setRadioEnabled(true);
      iccManager.addEventListener("iccdetected", function oniccdetected(evt) {
        log("icc iccdetected: " + evt.iccId);
        iccManager.removeEventListener("iccdetected", oniccdetected);
        taskHelper.runNext();
      });
    }
  });
});

// Start test
taskHelper.runNext();
