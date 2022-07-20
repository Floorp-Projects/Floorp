/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";
const UI_VERSION = 120;

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { PlacesUIUtils } = ChromeUtils.importESModule(
  "resource:///modules/PlacesUIUtils.sys.mjs"
);

add_task(async function has_not_used_ctrl_tab_and_its_off() {
  const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
    Ci.nsIObserver
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.migration.version");
  });
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);

  Services.xulStore.setValue(
    AppConstants.BROWSER_CHROME_URL,
    "place:test",
    "open",
    "true"
  );

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.equal(
    Services.xulStore.getValue(
      AppConstants.BROWSER_CHROME_URL,
      PlacesUIUtils.obfuscateUrlForXulStore("place:test"),
      "open"
    ),
    "true"
  );

  Assert.greater(
    Services.prefs.getIntPref("browser.migration.version"),
    UI_VERSION,
    "Check migration version has been bumped up"
  );
});
