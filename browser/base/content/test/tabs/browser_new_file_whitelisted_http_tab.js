/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_HTTP = "http://example.org/";

// Test for bug 1378377.
add_task(async function() {
  // Set prefs to ensure file content process.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.remote.separateFileUriProcess", true]],
  });

  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(fileBrowser) {
    ok(
      E10SUtils.isWebRemoteType(fileBrowser),
      "Check that tab normally has web remote type."
    );
  });

  // Set prefs to whitelist TEST_HTTP for file:// URI use.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["capability.policy.policynames", "allowFileURI"],
      ["capability.policy.allowFileURI.sites", TEST_HTTP],
      ["capability.policy.allowFileURI.checkloaduri.enabled", "allAccess"],
    ],
  });

  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(fileBrowser) {
    is(
      fileBrowser.remoteType,
      E10SUtils.FILE_REMOTE_TYPE,
      "Check that tab now has file remote type."
    );
  });
});
