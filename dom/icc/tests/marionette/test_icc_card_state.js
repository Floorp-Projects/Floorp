/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

function setRadioEnabled(enabled) {
  SpecialPowers.addPermission("settings-write", true, document);

  // TODO: Bug 856553 - [B2G] RIL: need an API to enable/disable radio
  let settings = navigator.mozSettings;
  let setLock = settings.createLock();
  let obj = {
    "ril.radio.disabled": !enabled
  };
  let setReq = setLock.set(obj);

  setReq.addEventListener("success", function onSetSuccess() {
    log("set 'ril.radio.disabled' to " + enabled);
  });

  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set 'ril.radio.disabled' to " + enabled);
  });

  SpecialPowers.removePermission("settings-write", document);
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
