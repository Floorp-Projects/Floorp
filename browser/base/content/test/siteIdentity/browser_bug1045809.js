// Test that the Mixed Content Doorhanger Action to re-enable protection works

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const TEST_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "file_bug1045809_1.html";

var origBlockActive;

add_task(async function() {
  registerCleanupFunction(function() {
    Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
    gBrowser.removeCurrentTab();
  });

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);

  // Make sure mixed content blocking is on
  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // Test 1: mixed content must be blocked
  await promiseTabLoadEvent(tab, TEST_URL);
  await test1(gBrowser.getBrowserForTab(tab));

  await promiseTabLoadEvent(tab);
  // Test 2: mixed content must NOT be blocked
  await test2(gBrowser.getBrowserForTab(tab));

  // Test 3: mixed content must be blocked again
  await promiseTabLoadEvent(tab);
  await test3(gBrowser.getBrowserForTab(tab));
});

async function test1(gTestBrowser) {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  await ContentTask.spawn(gTestBrowser, null, function() {
    var x = content.document.getElementsByTagName("iframe")[0].contentDocument.getElementById("mixedContentContainer");
    is(x, null, "Mixed Content is NOT to be found in Test1");
  });

  // Disable Mixed Content Protection for the page (and reload)
  gIdentityHandler.disableMixedContentProtection();
}

async function test2(gTestBrowser) {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});

  await ContentTask.spawn(gTestBrowser, null, function() {
    var x = content.document.getElementsByTagName("iframe")[0].contentDocument.getElementById("mixedContentContainer");
    isnot(x, null, "Mixed Content is to be found in Test2");
  });

  // Re-enable Mixed Content Protection for the page (and reload)
  gIdentityHandler.enableMixedContentProtection();
}

async function test3(gTestBrowser) {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  await ContentTask.spawn(gTestBrowser, null, function() {
    var x = content.document.getElementsByTagName("iframe")[0].contentDocument.getElementById("mixedContentContainer");
    is(x, null, "Mixed Content is NOT to be found in Test3");
  });
}
