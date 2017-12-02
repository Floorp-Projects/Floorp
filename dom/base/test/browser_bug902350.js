/*
 * Mixed Content Block frame navigates for target="_top" - Test for Bug 902350
 */

add_task(async function mixed_content_block_for_target_top_test() {
  const PREF_ACTIVE = "security.mixed_content.block_active_content";
  const httpsTestRoot = getRootDirectory(gTestPath)
    .replace("chrome://mochitests/content", "https://example.com");

  await SpecialPowers.pushPrefEnv({ set: [[ PREF_ACTIVE, true ]] });

  let newTab = await BrowserTestUtils.openNewForegroundTab({ gBrowser,
                                                             waitForLoad: true });
  let testBrowser = newTab.linkedBrowser;

  var url = httpsTestRoot + "file_bug902350.html";
  var frameUrl = httpsTestRoot + "file_bug902350_frame.html";
  let loadPromise = BrowserTestUtils.browserLoaded(testBrowser, false, url);
  let frameLoadPromise = BrowserTestUtils.browserLoaded(testBrowser, true,
                                                        frameUrl);
  testBrowser.loadURI(url);
  await loadPromise;
  await frameLoadPromise;

  // Find the iframe and click the link in it.
  let insecureUrl = "http://example.com/";
  let insecureLoadPromise = BrowserTestUtils.browserLoaded(testBrowser, false,
                                                           insecureUrl);
  ContentTask.spawn(testBrowser, null, function() {
    var frame = content.document.getElementById("testing_frame");
    var topTarget = frame.contentWindow.document.getElementById("topTarget");
    topTarget.click();
  });

  // Navigating to insecure domain through target='_top' should succeed.
  await insecureLoadPromise;

  // The link click should not invoke the Mixed Content Blocker.
  let {gIdentityHandler} = testBrowser.ownerGlobal;
  ok (!gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"),
      "Mixed Content Doorhanger did not appear when trying to navigate top");

  await BrowserTestUtils.removeTab(newTab);
});
