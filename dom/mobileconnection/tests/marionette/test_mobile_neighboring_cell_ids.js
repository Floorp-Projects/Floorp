/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head_chrome.js";

// Start test.
startTestBase(function() {
  // Emulator doesn't support REQUEST_GET_NEIGHBORING_CELL_IDS, so we expect to
  // get an 'RequestNotSupported' error here.
  return getNeighboringCellIds()
    .then(() => {
      ok(false, "should not success");
    }, (aErrorMsg) => {
      is(aErrorMsg, "RequestNotSupported",
         "Failed to getNeighboringCellIds: " + aErrorMsg);
    });
});
