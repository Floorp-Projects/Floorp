/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";
const IFRAME_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_iframe_page.html";

async function assertMenulist(entries, baseURL = TEST_PAGE) {
  // Wait for the session data to be flushed before continuing the test
  await new Promise(resolve =>
    SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
  );

  let backButton = document.getElementById("back-button");
  let contextMenu = document.getElementById("backForwardMenu");

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(backButton, {
    type: "contextmenu",
    button: 2,
  });
  await popupShownPromise;

  ok(true, "history menu opened");

  let nodes = contextMenu.childNodes;

  is(
    nodes.length,
    entries.length,
    "Has the expected number of contextMenu entries"
  );

  for (let i = 0; i < entries.length; i++) {
    let node = nodes[i];
    is(
      node.getAttribute("uri").replace(/[?|#]/, "!"),
      baseURL + "!entry=" + entries[i],
      "contextMenu node has the correct uri"
    );
  }

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}

// There are different ways of loading a page, but they should exhibit roughly the same
// back-forward behavior for the purpose of requiring user interaction. Thus, we
// have a utility function that runs the same test with a parameterized method of loading
// new URLs.
async function runTopLevelTest(loadMethod, useHashes = false) {
  let p = useHashes ? "#" : "?";

  // Test with both pref on and off
  for (let requireUserInteraction of [true, false]) {
    Services.prefs.setBoolPref(
      "browser.navigation.requireUserInteraction",
      requireUserInteraction
    );

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PAGE + p + "entry=0"
    );
    let browser = tab.linkedBrowser;

    assertBackForwardState(false, false);

    await loadMethod(TEST_PAGE + p + "entry=1");

    assertBackForwardState(true, false);
    await assertMenulist([1, 0]);

    await loadMethod(TEST_PAGE + p + "entry=2");

    assertBackForwardState(true, false);
    await assertMenulist(requireUserInteraction ? [2, 0] : [2, 1, 0]);

    await loadMethod(TEST_PAGE + p + "entry=3");

    info("Adding user interaction for entry=3");
    // Add some user interaction to entry 3
    await BrowserTestUtils.synthesizeMouse(
      "body",
      0,
      0,
      {},
      browser.browsingContext,
      true
    );

    assertBackForwardState(true, false);
    await assertMenulist(requireUserInteraction ? [3, 0] : [3, 2, 1, 0]);

    await loadMethod(TEST_PAGE + p + "entry=4");

    assertBackForwardState(true, false);
    await assertMenulist(requireUserInteraction ? [4, 3, 0] : [4, 3, 2, 1, 0]);

    info("Adding user interaction for entry=4");
    // Add some user interaction to entry 4
    await BrowserTestUtils.synthesizeMouse(
      "body",
      0,
      0,
      {},
      browser.browsingContext,
      true
    );

    await loadMethod(TEST_PAGE + p + "entry=5");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [5, 4, 3, 0] : [5, 4, 3, 2, 1, 0]
    );

    await goBack(TEST_PAGE + p + "entry=4");
    await goBack(TEST_PAGE + p + "entry=3");

    if (!requireUserInteraction) {
      await goBack(TEST_PAGE + p + "entry=2");
      await goBack(TEST_PAGE + p + "entry=1");
    }

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [5, 4, 3, 0] : [5, 4, 3, 2, 1, 0]
    );

    await goBack(TEST_PAGE + p + "entry=0");

    assertBackForwardState(false, true);

    if (!requireUserInteraction) {
      await goForward(TEST_PAGE + p + "entry=1");
      await goForward(TEST_PAGE + p + "entry=2");
    }

    await goForward(TEST_PAGE + p + "entry=3");

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [5, 4, 3, 0] : [5, 4, 3, 2, 1, 0]
    );

    await goForward(TEST_PAGE + p + "entry=4");

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [5, 4, 3, 0] : [5, 4, 3, 2, 1, 0]
    );

    await goForward(TEST_PAGE + p + "entry=5");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [5, 4, 3, 0] : [5, 4, 3, 2, 1, 0]
    );

    BrowserTestUtils.removeTab(tab);
  }

  Services.prefs.clearUserPref("browser.navigation.requireUserInteraction");
}

async function runIframeTest(loadMethod) {
  // Test with both pref on and off
  for (let requireUserInteraction of [true, false]) {
    Services.prefs.setBoolPref(
      "browser.navigation.requireUserInteraction",
      requireUserInteraction
    );

    // First test the boring case where we only have one iframe.
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      IFRAME_PAGE + "?entry=0"
    );
    let browser = tab.linkedBrowser;

    assertBackForwardState(false, false);

    await loadMethod(TEST_PAGE + "?sub_entry=1", "frame1");

    assertBackForwardState(true, false);
    await assertMenulist([0, 0], IFRAME_PAGE);

    await loadMethod(TEST_PAGE + "?sub_entry=2", "frame1");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [0, 0] : [0, 0, 0],
      IFRAME_PAGE
    );

    let bc = await SpecialPowers.spawn(browser, [], function () {
      return content.document.getElementById("frame1").browsingContext;
    });

    // Add some user interaction to sub entry 2
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, bc, true);

    await loadMethod(TEST_PAGE + "?sub_entry=3", "frame1");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [0, 0, 0] : [0, 0, 0, 0],
      IFRAME_PAGE
    );

    await loadMethod(TEST_PAGE + "?sub_entry=4", "frame1");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [0, 0, 0] : [0, 0, 0, 0, 0],
      IFRAME_PAGE
    );

    if (!requireUserInteraction) {
      await goBack(TEST_PAGE + "?sub_entry=3", true);
    }

    await goBack(TEST_PAGE + "?sub_entry=2", true);

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [0, 0, 0] : [0, 0, 0, 0, 0],
      IFRAME_PAGE
    );

    await loadMethod(IFRAME_PAGE + "?entry=1");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [1, 0, 0] : [1, 0, 0, 0],
      IFRAME_PAGE
    );

    BrowserTestUtils.removeTab(tab);

    // Two iframes, now we're talking.
    tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      IFRAME_PAGE + "?entry=0"
    );
    browser = tab.linkedBrowser;

    await loadMethod(IFRAME_PAGE + "?entry=1");

    assertBackForwardState(true, false);
    await assertMenulist(requireUserInteraction ? [1, 0] : [1, 0], IFRAME_PAGE);

    // Add some user interaction to frame 1.
    bc = await SpecialPowers.spawn(browser, [], function () {
      return content.document.getElementById("frame1").browsingContext;
    });
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, bc, true);

    // Add some user interaction to frame 2.
    bc = await SpecialPowers.spawn(browser, [], function () {
      return content.document.getElementById("frame2").browsingContext;
    });
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, bc, true);

    // Navigate frame 2.
    await loadMethod(TEST_PAGE + "?sub_entry=1", "frame2");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [1, 1, 0] : [1, 1, 0],
      IFRAME_PAGE
    );

    // Add some user interaction to frame 1, again.
    bc = await SpecialPowers.spawn(browser, [], function () {
      return content.document.getElementById("frame1").browsingContext;
    });
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, bc, true);

    // Navigate frame 2, again.
    await loadMethod(TEST_PAGE + "?sub_entry=2", "frame2");

    assertBackForwardState(true, false);
    await assertMenulist(
      requireUserInteraction ? [1, 1, 1, 0] : [1, 1, 1, 0],
      IFRAME_PAGE
    );

    await goBack(TEST_PAGE + "?sub_entry=1", true);

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [1, 1, 1, 0] : [1, 1, 1, 0],
      IFRAME_PAGE
    );

    await goBack(TEST_PAGE + "?sub_entry=0", true);

    assertBackForwardState(true, true);
    await assertMenulist(
      requireUserInteraction ? [1, 1, 1, 0] : [1, 1, 1, 0],
      IFRAME_PAGE
    );

    BrowserTestUtils.removeTab(tab);
  }

  Services.prefs.clearUserPref("browser.navigation.requireUserInteraction");
}

// Test that when the pref is flipped, we are skipping history
// entries without user interaction when following links with hash URIs.
add_task(async function test_hashURI() {
  async function followLinkHash(url) {
    info(`Creating and following a link to ${url}`);
    let browser = gBrowser.selectedBrowser;
    let loaded = BrowserTestUtils.waitForLocationChange(gBrowser, url);
    await SpecialPowers.spawn(browser, [url], function (url) {
      let a = content.document.createElement("a");
      a.href = url;
      content.document.body.appendChild(a);
      a.click();
    });
    await loaded;
    info(`Loaded ${url}`);
  }

  await runTopLevelTest(followLinkHash, true);
});

// Test that when the pref is flipped, we are skipping history
// entries without user interaction when using history.pushState.
add_task(async function test_pushState() {
  await runTopLevelTest(pushState);
});

// Test that when the pref is flipped, we are skipping history
// entries without user interaction when following a link.
add_task(async function test_followLink() {
  await runTopLevelTest(followLink);
});

// Test that when the pref is flipped, we are skipping history
// entries without user interaction when navigating inside an iframe
// using history.pushState.
add_task(async function test_iframe_pushState() {
  await runIframeTest(pushState);
});

// Test that when the pref is flipped, we are skipping history
// entries without user interaction when navigating inside an iframe
// by following links.
add_task(async function test_iframe_followLink() {
  await runIframeTest(followLink);
});
