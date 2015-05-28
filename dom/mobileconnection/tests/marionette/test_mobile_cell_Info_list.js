/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head_chrome.js";

function getAndroidVersion() {
  return runEmulatorShellCmdSafe(["getprop", "ro.build.version.sdk"])
    .then(aResults => aResults[0]);
}

// Start test.
startTestBase(function() {
  return getAndroidVersion().
    then((aVersion) => {
      if (aVersion < "19") {
        // Only emulator-kk supports REQUEST_GET_CELL_INFO_LIST, so we skip this
        // test if in older android version.
        log("Skip test: AndroidVersion: " + aVersion);
        return;
      }

      return getCellInfoList()
        .then((aResults) => {
          // Cell Info are hard-coded in hardware/ril/reference-ril/reference-ril.c.
          is(aResults.length, 1, "Check number of cell Info");

          let cell = aResults[0];
          is(cell.type, Ci.nsICellInfo.CELL_INFO_TYPE_GSM, "Check cell.type");
          is(cell.registered, true, "Check cell.registered");

          ok(cell instanceof Ci.nsIGsmCellInfo,
             "cell.constructor is " + cell.constructor);

          // The data hard-coded in hardware/ril/reference-ril/reference-ril.c
          // isn't correct (missing timeStampType), so we skip to check other
          // attributes first until we fix it.
        });
    });
});
