/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a HTTPS web page with active content from HTTP .onion URLs
// and makes sure that the mixed content flags on the docshell are not set.
//
// Note that the URLs referenced within the test page intentionally use the
// unassigned port 8 because we don't want to actually load anything, we just
// want to check that the URLs are not blocked.

const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "test_no_mcb_for_onions.html";

const PREF_BLOCK_DISPLAY = "security.mixed_content.block_display_content";
const PREF_BLOCK_ACTIVE = "security.mixed_content.block_active_content";
const PREF_ONION_WHITELIST = "dom.securecontext.whitelist_onions";

add_task(async function allowOnionMixedContent() {
  registerCleanupFunction(function() {
    gBrowser.removeCurrentTab();
  });

  await SpecialPowers.pushPrefEnv({ set: [[PREF_BLOCK_DISPLAY, true]] });
  await SpecialPowers.pushPrefEnv({ set: [[PREF_BLOCK_ACTIVE, true]] });
  await SpecialPowers.pushPrefEnv({ set: [[PREF_ONION_WHITELIST, true]] });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_URL
  ).catch(Cu.reportError);
  const browser = gBrowser.getBrowserForTab(tab);

  await ContentTask.spawn(browser, null, function() {
    is(
      docShell.hasMixedDisplayContentBlocked,
      false,
      "hasMixedDisplayContentBlocked not set"
    );
    is(
      docShell.hasMixedActiveContentBlocked,
      false,
      "hasMixedActiveContentBlocked not set"
    );
  });

  await assertMixedContentBlockingState(browser, {
    activeBlocked: false,
    activeLoaded: false,
    passiveLoaded: false,
  });
});
