/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AttributionCode } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);

// Keep in sync with `BROWSER_ATTRIBUTION_ERRORS` in Histograms.json.
const INDEX_READ_ERROR = 0;
const INDEX_DECODE_ERROR = 1;
const INDEX_WRITE_ERROR = 2;
const INDEX_QUARANTINE_ERROR = 3;

add_setup(function () {
  // AttributionCode._clearCache is only possible in a testing environment
  Services.env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");

  registerCleanupFunction(() => {
    Services.env.set("XPCSHELL_TEST_PROFILE_DIR", null);
  });
});
