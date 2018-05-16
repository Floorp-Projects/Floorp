const UI_VERSION = 69;
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";

var gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);

Services.prefs.setIntPref("browser.migration.version", UI_VERSION - 1);

add_task(async function test_check_cleanup_social_prefs() {
  Services.prefs.setStringPref("social.manifest.example-com", "example.com");

  // Simulate a migration.
  gBrowserGlue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_BROWSERGLUE_TEST);

  Assert.ok(!Services.prefs.prefHasUserValue("social.manifest.example-com"),
            "should have cleared old social preference 'social.manifest.example-com'");
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.migration.version");
  Services.prefs.clearUserPref("social.manifest.example-com");
});
