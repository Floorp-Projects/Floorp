/*
 * User Override Mixed Content Block - Tests for Bug 822367
 */

const PREF_DISPLAY = "security.mixed_content.block_display_content";
const PREF_DISPLAY_UPGRADE = "security.mixed_content.upgrade_display_content";
const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We alternate for even and odd test cases to simulate different hosts
const HTTPS_TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const HTTPS_TEST_ROOT_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test1.example.com"
);

var gTestBrowser = null;

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_DISPLAY, true],
      [PREF_DISPLAY_UPGRADE, false],
      [PREF_ACTIVE, true],
    ],
  });

  var newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop();

  // Mixed Script Test
  var url = HTTPS_TEST_ROOT + "file_bug822367_1.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);
});

// Mixed Script Test
add_task(async function MixedTest1A() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  gTestBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest1B() {
  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 1"
    );
  });
  gTestBrowser.ownerGlobal.gIdentityHandler.enableMixedContentProtectionNoReload();
});

// Mixed Display Test - Doorhanger should not appear
add_task(async function MixedTest2() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_2.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);

  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: false,
    passiveLoaded: false,
  });
});

// Mixed Script and Display Test - User Override should cause both the script and the image to load.
add_task(async function MixedTest3() {
  var url = HTTPS_TEST_ROOT + "file_bug822367_3.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);
});

add_task(async function MixedTest3A() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  gTestBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest3B() {
  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let p1 = ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 3"
    );
    let p2 = ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p2").innerHTML == "bye",
      "Waited too long for mixed image to load in Test 3"
    );
    await Promise.all([p1, p2]);
  });

  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: true,
    activeBlocked: false,
    passiveLoaded: true,
  });
  gTestBrowser.ownerGlobal.gIdentityHandler.enableMixedContentProtectionNoReload();
});

// Location change - User override on one page doesn't propagate to another page after location change.
add_task(async function MixedTest4() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_4.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);
});

let preLocationChangePrincipal = null;
add_task(async function MixedTest4A() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  preLocationChangePrincipal = gTestBrowser.contentPrincipal;
  gTestBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest4B() {
  let url = HTTPS_TEST_ROOT + "file_bug822367_4B.html";
  await SpecialPowers.spawn(gTestBrowser, [url], async function(wantedUrl) {
    await ContentTaskUtils.waitForCondition(
      () => content.document.location == wantedUrl,
      "Waited too long for mixed script to run in Test 4"
    );
  });
});

add_task(async function MixedTest4C() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "",
      "Mixed script loaded in test 4 after location change!"
    );
  });
  SitePermissions.removeFromPrincipal(
    preLocationChangePrincipal,
    "mixed-content"
  );
});

// Mixed script attempts to load in a document.open()
add_task(async function MixedTest5() {
  var url = HTTPS_TEST_ROOT + "file_bug822367_5.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);
});

add_task(async function MixedTest5A() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  gTestBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest5B() {
  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 5"
    );
  });
  gTestBrowser.ownerGlobal.gIdentityHandler.enableMixedContentProtectionNoReload();
});

// Mixed script attempts to load in a document.open() that is within an iframe.
add_task(async function MixedTest6() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_6.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser, false, url);
});

add_task(async function MixedTest6A() {
  gTestBrowser.removeEventListener("load", MixedTest6A, true);
  let { gIdentityHandler } = gTestBrowser.ownerGlobal;

  await BrowserTestUtils.waitForCondition(
    () =>
      gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"),
    "Waited too long for control center to get mixed active blocked state"
  );
});

add_task(async function MixedTest6B() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: true,
    passiveLoaded: false,
  });

  gTestBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();

  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest6C() {
  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    function test() {
      try {
        return (
          content.document
            .getElementById("f1")
            .contentDocument.getElementById("p1").innerHTML == "hello"
        );
      } catch (e) {
        return false;
      }
    }

    await ContentTaskUtils.waitForCondition(
      test,
      "Waited too long for mixed script to run in Test 6"
    );
  });
});

add_task(async function MixedTest6D() {
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: true,
    activeBlocked: false,
    passiveLoaded: false,
  });
  gTestBrowser.ownerGlobal.gIdentityHandler.enableMixedContentProtectionNoReload();
});

add_task(async function cleanup() {
  gBrowser.removeCurrentTab();
});
