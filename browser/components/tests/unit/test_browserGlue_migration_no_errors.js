/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Since various migrations in BrowserGlue are sometimes trivial, e.g. just
 * clearing a pref, it does not feel necessary to write tests for each of those.
 *
 * However, ensuring we have at least some coverage to check for errors, e.g.
 * typos, is a good idea, hence this test.
 *
 * If your migration is more complex that clearing a couple of prefs, you
 * should consider adding your own BrowserGlue migration test.
 */
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

// Set the migration value to 1, to ensure that the migration code is called,
// and so that this doesn't need updating every time we obsolete old tests.
Services.prefs.setIntPref("browser.migration.version", 1);

add_task(async function test_no_errors() {
  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.ok(true, "should have run the migration with no errors");
});
