/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // Basic test.
  is(icc.cardState, "ready", "card state is " + icc.cardState);

  // Test cardstatechange event by switching radio off.
  return Promise.resolve()
    // Turn off radio and expect to get card state changing to null.
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(false));
      promises.push(waitForTargetEvent(icc, "cardstatechange", function() {
        return icc.cardState === null;
      }));
      return Promise.all(promises);
    })
    // Restore radio status and expect to get iccdetected event.
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(true));
      promises.push(waitForTargetEvent(iccManager, "iccdetected"));
      return Promise.all(promises);
    });
});
