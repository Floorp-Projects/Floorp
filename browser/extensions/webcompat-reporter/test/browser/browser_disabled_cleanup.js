// Test the addon is cleaning up after itself when disabled.
add_task(async function test_disabled() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_WC_REPORTER_ENABLED, false]]});

  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, function() {
    is(typeof window._webCompatReporterTabListener, "undefined", "TabListener expando does not exist.");
    is(document.getElementById("webcompat-reporter-button"), null, "Report Site Issue button does not exist.");
  });
});
