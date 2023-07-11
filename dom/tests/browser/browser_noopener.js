"use strict";

const TESTS = [
  { id: "#test1", name: "", opener: true, newWindow: false },
  { id: "#test2", name: "", opener: false, newWindow: false },
  { id: "#test3", name: "", opener: false, newWindow: false },

  { id: "#test4", name: "uniquename1", opener: true, newWindow: false },
  { id: "#test5", name: "uniquename2", opener: false, newWindow: false },
  { id: "#test6", name: "uniquename3", opener: false, newWindow: false },

  { id: "#test7", name: "", opener: true, newWindow: false },
  { id: "#test8", name: "", opener: false, newWindow: false },
  { id: "#test9", name: "", opener: false, newWindow: false },

  { id: "#test10", name: "uniquename1", opener: true, newWindow: false },
  { id: "#test11", name: "uniquename2", opener: false, newWindow: false },
  { id: "#test12", name: "uniquename3", opener: false, newWindow: false },
];

const TEST_URL =
  "http://mochi.test:8888/browser/dom/tests/browser/test_noopener_source.html";
const TARGET_URL =
  "http://mochi.test:8888/browser/dom/tests/browser/test_noopener_target.html";

const OPEN_NEWWINDOW_PREF = "browser.link.open_newwindow";
const OPEN_NEWWINDOW = 2;
const OPEN_NEWTAB = 3;

const NOOPENER_NEWPROC_PREF = "dom.noopener.newprocess.enabled";

async function doTests(usePrivate, container) {
  let alwaysNewWindow =
    SpecialPowers.getIntPref(OPEN_NEWWINDOW_PREF) == OPEN_NEWWINDOW;

  let window = await BrowserTestUtils.openNewBrowserWindow({
    private: usePrivate,
  });

  let tabOpenOptions = {};
  if (container) {
    tabOpenOptions.userContextId = 1;
  }

  for (let test of TESTS) {
    const testid = `${test.id} (private=${usePrivate}, container=${container}, alwaysNewWindow=${alwaysNewWindow})`;
    let originalTab = BrowserTestUtils.addTab(
      window.gBrowser,
      TEST_URL,
      tabOpenOptions
    );
    await BrowserTestUtils.browserLoaded(originalTab.linkedBrowser);
    await BrowserTestUtils.switchTab(window.gBrowser, originalTab);

    let waitFor;
    if (test.newWindow || alwaysNewWindow) {
      waitFor = BrowserTestUtils.waitForNewWindow({ url: TARGET_URL });
      // Confirm that this window has private browsing set if we're doing a private browsing test
    } else {
      waitFor = BrowserTestUtils.waitForNewTab(
        window.gBrowser,
        TARGET_URL,
        true
      );
    }

    BrowserTestUtils.synthesizeMouseAtCenter(
      test.id,
      {},
      window.gBrowser.getBrowserForTab(originalTab)
    );

    let tab;
    if (test.newWindow || alwaysNewWindow) {
      let window = await waitFor;
      is(
        PrivateBrowsingUtils.isWindowPrivate(window),
        usePrivate,
        "Private status should match for " + testid
      );
      tab = window.gBrowser.selectedTab;
    } else {
      tab = await waitFor;
    }

    // Check that the name matches.
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [test, container, testid],
      async (test, container, testid) => {
        Assert.equal(
          content.document.nodePrincipal.originAttributes.userContextId,
          container ? 1 : 0,
          `User context ID should match for ${testid}`
        );

        Assert.equal(
          content.window.name,
          test.name,
          `Name should match for ${testid}`
        );
        if (test.opener) {
          Assert.ok(
            content.window.opener,
            `Opener should have been set for ${testid}`
          );
        } else {
          Assert.ok(
            !content.window.opener,
            `Opener should not have been set for ${testid}`
          );
        }
      }
    );

    BrowserTestUtils.removeTab(tab);
    BrowserTestUtils.removeTab(originalTab);
  }

  window.close();
}

async function doAllTests() {
  // Non-private window
  await doTests(false, false);

  // Private window
  await doTests(true, false);

  // Non-private window with container
  await doTests(false, true);
}

// This test takes a really long time, especially in debug builds, as it is
// constant starting and stopping processes, and opens a new window ~144 times.
requestLongerTimeout(30);

add_task(async function newtab_sameproc() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [OPEN_NEWWINDOW_PREF, OPEN_NEWTAB],
      [NOOPENER_NEWPROC_PREF, false],
    ],
  });
  await doAllTests();
});

add_task(async function newtab_newproc() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [OPEN_NEWWINDOW_PREF, OPEN_NEWTAB],
      [NOOPENER_NEWPROC_PREF, true],
    ],
  });
  await doAllTests();
});

add_task(async function newwindow_sameproc() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [OPEN_NEWWINDOW_PREF, OPEN_NEWWINDOW],
      [NOOPENER_NEWPROC_PREF, false],
    ],
  });
  await doAllTests();
});

add_task(async function newwindow_newproc() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [OPEN_NEWWINDOW_PREF, OPEN_NEWWINDOW],
      [NOOPENER_NEWPROC_PREF, true],
    ],
  });
  await doAllTests();
});
