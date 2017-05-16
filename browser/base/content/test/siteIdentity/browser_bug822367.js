/*
 * User Override Mixed Content Block - Tests for Bug 822367
 */


const PREF_DISPLAY = "security.mixed_content.block_display_content";
const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We alternate for even and odd test cases to simulate different hosts
const HTTPS_TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com");
const HTTPS_TEST_ROOT_2 = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://test1.example.com");

var gTestBrowser = null;

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({ set: [[ PREF_DISPLAY, true ],
                                          [ PREF_ACTIVE, true  ]] });

  var newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop()

  // Mixed Script Test
  var url = HTTPS_TEST_ROOT + "file_bug822367_1.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

// Mixed Script Test
add_task(async function MixedTest1A() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest1B() {
  await ContentTask.spawn(gTestBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 1");
  });
});

// Mixed Display Test - Doorhanger should not appear
add_task(async function MixedTest2() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_2.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);

  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: false});
});

// Mixed Script and Display Test - User Override should cause both the script and the image to load.
add_task(async function MixedTest3() {
  var url = HTTPS_TEST_ROOT + "file_bug822367_3.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest3A() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest3B() {
  await ContentTask.spawn(gTestBrowser, null, async function() {
    let p1 = ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 3");
    let p2 = ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p2").innerHTML == "bye",
      "Waited too long for mixed image to load in Test 3");
    await Promise.all([ p1, p2 ]);
  });

  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: true});
});

// Location change - User override on one page doesn't propogate to another page after location change.
add_task(async function MixedTest4() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_4.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest4A() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest4B() {
  let url = HTTPS_TEST_ROOT + "file_bug822367_4B.html";
  await ContentTask.spawn(gTestBrowser, url, async function(wantedUrl) {
    await ContentTaskUtils.waitForCondition(
      () => content.document.location == wantedUrl,
      "Waited too long for mixed script to run in Test 4");
  });
});

add_task(async function MixedTest4C() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  await ContentTask.spawn(gTestBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "",
      "Mixed script loaded in test 4 after location change!");
  });
});

// Mixed script attempts to load in a document.open()
add_task(async function MixedTest5() {
  var url = HTTPS_TEST_ROOT + "file_bug822367_5.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest5A() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest5B() {
  await ContentTask.spawn(gTestBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("p1").innerHTML == "hello",
      "Waited too long for mixed script to run in Test 5");
  });
});

// Mixed script attempts to load in a document.open() that is within an iframe.
add_task(async function MixedTest6() {
  var url = HTTPS_TEST_ROOT_2 + "file_bug822367_6.html";
  BrowserTestUtils.loadURI(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest6A() {
  gTestBrowser.removeEventListener("load", MixedTest6A, true);
  let {gIdentityHandler} = gTestBrowser.ownerGlobal;

  await BrowserTestUtils.waitForCondition(
    () => gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"),
    "Waited too long for control center to get mixed active blocked state");
});

add_task(async function MixedTest6B() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();

  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function MixedTest6C() {
  await ContentTask.spawn(gTestBrowser, null, async function() {
    function test() {
      try {
        return content.document.getElementById("f1").contentDocument.getElementById("p1").innerHTML == "hello";
      } catch (e) {
        return false;
      }
    }

    await ContentTaskUtils.waitForCondition(test, "Waited too long for mixed script to run in Test 6");
  });
});

add_task(async function MixedTest6D() {
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});
});

add_task(async function cleanup() {
  gBrowser.removeCurrentTab();
});
