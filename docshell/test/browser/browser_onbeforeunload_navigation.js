"use strict";

const TEST_PAGE =
  "http://mochi.test:8888/browser/docshell/test/browser/file_bug1046022.html";
const TARGETED_PAGE =
  "data:text/html," +
  encodeURIComponent("<body>Shouldn't be seeing this</body>");

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

var loadStarted = false;
var tabStateListener = {
  resolveLoad: null,
  expectLoad: null,

  onStateChange(webprogress, request, flags, status) {
    const WPL = Ci.nsIWebProgressListener;
    if (flags & WPL.STATE_IS_WINDOW) {
      if (flags & WPL.STATE_START) {
        loadStarted = true;
      } else if (flags & WPL.STATE_STOP) {
        let url = request.QueryInterface(Ci.nsIChannel).URI.spec;
        is(url, this.expectLoad, "Should only see expected document loads");
        if (url == this.expectLoad) {
          this.resolveLoad();
        }
      }
    }
  },
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),
};

function promiseLoaded(url, callback) {
  if (tabStateListener.expectLoad) {
    throw new Error("Can't wait for multiple loads at once");
  }
  tabStateListener.expectLoad = url;
  return new Promise(resolve => {
    tabStateListener.resolveLoad = resolve;
    if (callback) {
      callback();
    }
  }).then(() => {
    tabStateListener.expectLoad = null;
    tabStateListener.resolveLoad = null;
  });
}

function promiseStayOnPagePrompt(browser, acceptNavigation) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "confirmEx" },
    { buttonNumClick: acceptNavigation ? 0 : 1 }
  );
}

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE,
    false,
    true
  );
  let browser = testTab.linkedBrowser;
  browser.addProgressListener(
    tabStateListener,
    Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
  );

  const NUM_TESTS = 7;
  await SpecialPowers.spawn(browser, [NUM_TESTS], testCount => {
    let { testFns } = this.content.wrappedJSObject;
    Assert.equal(
      testFns.length,
      testCount,
      "Should have the correct number of test functions"
    );
  });

  for (let allowNavigation of [false, true]) {
    for (let i = 0; i < NUM_TESTS; i++) {
      info(
        `Running test ${i} with navigation ${
          allowNavigation ? "allowed" : "forbidden"
        }`
      );

      if (allowNavigation) {
        // If we're allowing navigations, we need to re-load the test
        // page after each test, since the tests will each navigate away
        // from it.
        await promiseLoaded(TEST_PAGE, () => {
          browser.loadURI(TEST_PAGE, {
            triggeringPrincipal: document.nodePrincipal,
          });
        });
      }

      let promptPromise = promiseStayOnPagePrompt(browser, allowNavigation);
      let loadPromise;
      if (allowNavigation) {
        loadPromise = promiseLoaded(TARGETED_PAGE);
      }

      let winID = await SpecialPowers.spawn(
        browser,
        [i, TARGETED_PAGE],
        (testIdx, url) => {
          let { testFns } = this.content.wrappedJSObject;
          this.content.onbeforeunload = testFns[testIdx];
          this.content.location = url;
          return this.content.windowGlobalChild.innerWindowId;
        }
      );

      await promptPromise;
      await loadPromise;

      if (allowNavigation) {
        await SpecialPowers.spawn(
          browser,
          [TARGETED_PAGE, winID],
          (url, winID) => {
            this.content.onbeforeunload = null;
            Assert.equal(
              this.content.location.href,
              url,
              "Page should have navigated to the correct URL"
            );
            Assert.notEqual(
              this.content.windowGlobalChild.innerWindowId,
              winID,
              "Page should have a new inner window"
            );
          }
        );
      } else {
        await SpecialPowers.spawn(browser, [TEST_PAGE, winID], (url, winID) => {
          this.content.onbeforeunload = null;
          Assert.equal(
            this.content.location.href,
            url,
            "Page should have the same URL"
          );
          Assert.equal(
            this.content.windowGlobalChild.innerWindowId,
            winID,
            "Page should have the same inner window"
          );
        });
      }
    }
  }

  gBrowser.removeTab(testTab);
});
