// Test that the Mixed Content Doorhanger Action to re-enable protection works

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const PREF_INSECURE = "security.insecure_connection_icon.enabled";
const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_bug1045809_1.html";

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

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

  // Check with insecure lock disabled
  await SpecialPowers.pushPrefEnv({ set: [[PREF_INSECURE, false]] });
  await runTests(tab);

  // Check with insecure lock disabled
  await SpecialPowers.pushPrefEnv({ set: [[PREF_INSECURE, true]] });
  await runTests(tab);
});

async function runTests(tab) {
  // Test 1: mixed content must be blocked
  await promiseTabLoadEvent(tab, TEST_URL);
  await test1(gBrowser.getBrowserForTab(tab));

  await promiseTabLoadEvent(tab);
  // Test 2: mixed content must NOT be blocked
  await test2(gBrowser.getBrowserForTab(tab));

  // Test 3: mixed content must be blocked again
  await promiseTabLoadEvent(tab);
  await test3(gBrowser.getBrowserForTab(tab));
}

async function test1(gTestBrowser) {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  await SpecialPowers.spawn(gTestBrowser, [], function() {
    let iframe = content.document.getElementsByTagName("iframe")[0];

    SpecialPowers.spawn(iframe, [], () => {
      let container = content.document.getElementById("mixedContentContainer");
      is(container, null, "Mixed Content is NOT to be found in Test1");
    });
  });

  // Disable Mixed Content Protection for the page (and reload)
  gIdentityHandler.disableMixedContentProtection();
}

async function test2(gTestBrowser) {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: true,
    activeBlocked: false,
    passiveLoaded: false,
  });

  await SpecialPowers.spawn(gTestBrowser, [], function() {
    let iframe = content.document.getElementsByTagName("iframe")[0];

    SpecialPowers.spawn(iframe, [], () => {
      let container = content.document.getElementById("mixedContentContainer");
      isnot(container, null, "Mixed Content is to be found in Test2");
    });
  });

  // Re-enable Mixed Content Protection for the page (and reload)
  gIdentityHandler.enableMixedContentProtection();
}

async function test3(gTestBrowser) {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  await SpecialPowers.spawn(gTestBrowser, [], function() {
    let iframe = content.document.getElementsByTagName("iframe")[0];

    SpecialPowers.spawn(iframe, [], () => {
      let container = content.document.getElementById("mixedContentContainer");
      is(container, null, "Mixed Content is NOT to be found in Test3");
    });
  });
}
