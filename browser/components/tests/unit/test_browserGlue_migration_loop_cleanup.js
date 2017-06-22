const UI_VERSION = 41;
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";

var gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);

Services.prefs.setIntPref("browser.migration.version", UI_VERSION - 1);

add_task(async function test_check_cleanup_loop_prefs() {
  Services.prefs.setBoolPref("loop.createdRoom", true);
  Services.prefs.setBoolPref("loop1.createdRoom", true);
  Services.prefs.setBoolPref("loo.createdRoom", true);

  // Simulate a migration.
  gBrowserGlue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_BROWSERGLUE_TEST);

  Assert.throws(() => Services.prefs.getBoolPref("loop.createdRoom"),
                /NS_ERROR_UNEXPECTED/,
                "should have cleared old loop preference 'loop.createdRoom'");
  Assert.ok(Services.prefs.getBoolPref("loop1.createdRoom"),
            "should have left non-loop pref 'loop1.createdRoom' untouched");
  Assert.ok(Services.prefs.getBoolPref("loo.createdRoom"),
            "should have left non-loop pref 'loo.createdRoom' untouched");
});

do_register_cleanup(() => {
  Services.prefs.clearUserPref("browser.migration.version");
  Services.prefs.clearUserPref("loop.createdRoom");
  Services.prefs.clearUserPref("loop1.createdRoom");
  Services.prefs.clearUserPref("loo.createdRoom");
});
