/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

// Keep in sync with `BROWSER_ATTRIBUTION_ERRORS` in Histograms.json.
const INDEX_READ_ERROR = 0;
const INDEX_DECODE_ERROR = 1;
const INDEX_WRITE_ERROR = 2;
const INDEX_QUARANTINE_ERROR = 3;

add_task(function setup() {
  // AttributionCode._clearCache is only possible in a testing environment
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");

  registerCleanupFunction(() => {
    env.set("XPCSHELL_TEST_PROFILE_DIR", null);
  });
});
