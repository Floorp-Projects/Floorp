/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a HTTPS web page with active content from HTTP loopback URLs
// and makes sure that the mixed content flags on the docshell are not set.
//
// Note that the URLs referenced within the test page intentionally use the
// unassigned port 8 because we don't want to actually load anything, we just
// want to check that the URLs are not blocked.

const TEST_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "test_no_mcb_for_loopback.html";

const LOOPBACK_PNG_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://127.0.0.1:8888") + "moz.png";

const PREF_BLOCK_DISPLAY = "security.mixed_content.block_display_content";
const PREF_BLOCK_ACTIVE = "security.mixed_content.block_active_content";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF_BLOCK_DISPLAY);
  Services.prefs.clearUserPref(PREF_BLOCK_ACTIVE);
  gBrowser.removeCurrentTab();
});

add_task(async function allowLoopbackMixedContent() {
  Services.prefs.setBoolPref(PREF_BLOCK_DISPLAY, true);
  Services.prefs.setBoolPref(PREF_BLOCK_ACTIVE, true);

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  const browser = gBrowser.getBrowserForTab(tab);

  await ContentTask.spawn(browser, null, function() {
    is(docShell.hasMixedDisplayContentBlocked, false, "hasMixedDisplayContentBlocked not set");
    is(docShell.hasMixedActiveContentBlocked, false, "hasMixedActiveContentBlocked not set");
  });

  // Check that loopback content served from the cache is not blocked.
  await ContentTask.spawn(browser, LOOPBACK_PNG_URL, async function(loopbackPNGUrl) {
    const doc = content.document;
    const img = doc.createElement("img");
    const promiseImgLoaded = ContentTaskUtils.waitForEvent(img, "load", false);
    img.src = loopbackPNGUrl;
    Assert.ok(!img.complete, "loopback image not yet loaded");
    doc.body.appendChild(img);
    await promiseImgLoaded;

    const cachedImg = doc.createElement("img");
    cachedImg.src = img.src;
    Assert.ok(cachedImg.complete, "loopback image loaded from cache");
  });

  assertMixedContentBlockingState(browser, {
    activeBlocked: false,
    activeLoaded: false,
    passiveLoaded: false,
  });
});
