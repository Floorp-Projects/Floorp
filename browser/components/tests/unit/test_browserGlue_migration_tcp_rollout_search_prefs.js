const UI_VERSION = 127;
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";

var gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

Services.prefs.setIntPref("browser.migration.version", UI_VERSION - 1);

const PREFS_TO_CLEAR = {
  "browser.search.param.google_channel_us": "tus7",
  "browser.search.param.google_channel_row": "trow7",
  "browser.search.param.bing_ptag": "MOZZ0000000031",
};

const PREFS_TO_KEEP = {
  "browser.search.param.google_channel_us": "tus99",
  "browser.search.param.google_channel_row": "trow99",
  "browser.search.param.bing_ptag": "MOZZ0000000099",
  "browser.search.param.foo_bar": "MOZZ0000000031",
};

add_task(async function test_pref_migration_clear() {
  Object.entries(PREFS_TO_CLEAR).forEach(([key, value]) => {
    Services.prefs.setStringPref(key, value);
  });

  Object.entries(PREFS_TO_CLEAR).forEach(([key, value]) => {
    Assert.equal(
      Services.prefs.getStringPref(key, null),
      value,
      "Should set pref " + key
    );
  });

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Object.keys(PREFS_TO_CLEAR).forEach(key => {
    Assert.ok(
      !Services.prefs.prefHasUserValue(key),
      "Should have removed pref " + key
    );
  });
});

add_task(async function test_pref_migration_keep() {
  Object.entries(PREFS_TO_KEEP).forEach(([key, value]) => {
    Services.prefs.setStringPref(key, value);
  });

  Object.entries(PREFS_TO_KEEP).forEach(([key, value]) => {
    Assert.equal(
      Services.prefs.getStringPref(key, null),
      value,
      "Should set pref " + key
    );
  });

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Object.entries(PREFS_TO_KEEP).forEach(([key, value]) => {
    Assert.equal(
      Services.prefs.getStringPref(key, null),
      value,
      "Should have kept pref " + key
    );
  });
});

registerCleanupFunction(() => {
  Object.keys(PREFS_TO_CLEAR)
    .concat(Object.keys(PREFS_TO_KEEP))
    .forEach(Services.prefs.clearUserPref);
});
