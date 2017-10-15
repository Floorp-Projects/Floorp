"use strict";

const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

// Keep this in sync with the order in Histograms.json for
// BUSY_TAB_ABANDONED
const CATEGORIES = [
  "stop",
  "back",
  "forward",
  "historyNavigation",
  "reload",
  "tabClosed",
  "newURI",
];

const PAGE_2 = "data:text/html,<html>Page 2</html>";

/**
 * This little framework helps to extract the unique things
 * involved in testing each category of the BUSY_TAB_ABANDONED
 * probe from all of the common things. The little framework
 * in this test will take each item in PROBE_TEST, let it
 * do some test preparation, then create a "busy" tab to be
 * manipulated by the test. The "category" of the test will then
 * be used to determine if the appropriate index in the
 * histogram was bumped.
 *
 * Here's a more verbose breakdown of what each PROBE_TEST
 * should supply:
 *
 * name (String):
 *   Human readable description of the test. This is dumped
 *   out via info().
 *
 * category (String):
 *   The string representation of the index that will get
 *   incremented in the BUSY_TAB_ABANDONED probe. This should
 *   be a value inside CATEGORIES.
 *
 * prepare (function*)
 *   A function that can yield Promises. This will be run once
 *   we have a brand new browser to deal with, and should be used by
 *   the PROBE_TEST to do any history preparation before the browser
 *   is made to appear "busy" and is tested.
 *
 *   @param browser (<xul:browser>)
 *          The newly created browser that the test will use.
 *
 * doAction (function*)
 *   A function that can yield Promises. This will be run once
 *   the browser appears busy, and should cause the user action
 *   that will change the BUSY_TAB_ABANDONED probe.
 *
 *   @param browser (<xul:browser>)
 *          The busy browser to perform the action on.
 */
const PROBE_TESTS = [

  // Test stopping the tab
  {
    name: "Stopping the browser",

    category: "stop",

    prepare(browser) {},

    doAction(browser) {
      document.getElementById("Browser:Stop").doCommand();
    },
  },

  // Test going back to a previous page
  {
    name: "Going back to a previous page",

    category: "back",

    async prepare(browser) {
      browser.loadURI(PAGE_2);
      await BrowserTestUtils.browserLoaded(browser);
    },

    async doAction(browser) {
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      document.getElementById("Browser:Back").doCommand();
      await pageShow;
    },
  },

  // Test going forward to a previous page
  {
    name: "Going forward to the next page",

    category: "forward",

    async prepare(browser) {
      browser.loadURI(PAGE_2);
      await BrowserTestUtils.browserLoaded(browser);
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      browser.goBack();
      await pageShow;
    },

    async doAction(browser) {
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      document.getElementById("Browser:Forward").doCommand();
      await pageShow;
    },
  },

  // Test going backwards more than one page back via gotoIndex
  {
    name: "Going backward to a previous page via gotoIndex",

    category: "historyNavigation",

    async prepare(browser) {
      browser.loadURI(PAGE_2);
      await BrowserTestUtils.browserLoaded(browser);
      browser.loadURI("http://example.com");
      await BrowserTestUtils.browserLoaded(browser);
      await TabStateFlusher.flush(browser);
    },

    async doAction(browser) {
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      synthesizeHistoryNavigationToIndex(0);
      await pageShow;
    },
  },

  // Test going forwards more than one page back via gotoIndex
  {
    name: "Going forward to a previous page via gotoIndex",

    category: "historyNavigation",

    async prepare(browser) {
      browser.loadURI(PAGE_2);
      await BrowserTestUtils.browserLoaded(browser);
      browser.loadURI("http://example.com");
      await BrowserTestUtils.browserLoaded(browser);
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      browser.gotoIndex(0);
      await pageShow;
      await TabStateFlusher.flush(browser);
    },

    async doAction(browser) {
      let pageShow =
        BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      synthesizeHistoryNavigationToIndex(2);
      await pageShow;
    },
  },

  // Test reloading the tab
  {
    name: "Reloading the browser",

    category: "reload",

    prepare(browser) {},

    async doAction(browser) {
      document.getElementById("Browser:Reload").doCommand();
      await BrowserTestUtils.browserLoaded(browser);
    },
  },

  // Testing closing the tab is done in its own test later on
  // in this file.

  // Test browsing to a new URL
  {
    name: "Browsing to a new URL",

    category: "newURI",

    prepare(browser) {},

    async doAction(browser) {
      openUILinkIn(PAGE_2, "current");
      await BrowserTestUtils.browserLoaded(browser);
    },
  },
];

/**
 * Takes a Telemetry histogram snapshot and makes sure
 * that the index for that value (as defined by CATEGORIES)
 * has a count of 1, and that it's the only value that
 * has been incremented.
 *
 * @param snapshot (Object)
 *        The Telemetry histogram snapshot to examine.
 * @param category (String)
 *        The category in CATEGORIES whose index we expect to have
 *        been set to 1.
 */
function assertOnlyOneTypeSet(snapshot, category) {
  let categoryIndex = CATEGORIES.indexOf(category);
  Assert.equal(snapshot.counts[categoryIndex], 1,
               `Should have seen the ${category} count increment.`);
  // Use Array.prototype.reduce to sum up all of the
  // snapshot.count entries
  Assert.equal(snapshot.counts.reduce((a, b) => a + b), 1,
               "Should only be 1 collected value.");
}

/**
 * A helper function for simulating clicking on the history
 * navigation popup menu that you get if you click and hold
 * on the back or forward buttons when you have some browsing
 * history.
 *
 * @param index (int)
 *        The index for the menuitem we want to simulate
 *        clicking on.
 */
function synthesizeHistoryNavigationToIndex(index) {
  let popup = document.getElementById("backForwardMenu");
  // I don't want to deal with popup listening - that's
  // notoriously flake-y in automated tests. I'll just
  // directly call the function that populates the menu.
  FillHistoryMenu(popup);
  Assert.ok(popup.childElementCount > 0,
            "Should have some items in the back/forward menu");
  let menuitem = popup.querySelector(`menuitem[index="${index}"]`);
  Assert.ok(menuitem, `Should find a menuitem with index ${index}`);
  // Now pretend we clicked on the right item.
  let cmdEvent = new CustomEvent("command", {
    bubbles: true,
    cancelable: true,
  });
  menuitem.dispatchEvent(cmdEvent);
}

/**
 * Goes through each of the categories for the BUSY_TAB_ABANDONED
 * probe, and tests that they're properly changed.
 */
add_task(async function test_probes() {
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  let histogram = Services.telemetry
                          .getHistogramById("BUSY_TAB_ABANDONED");

  // If you want to add new tests for probes that don't involve
  // the tab or window hosting the tab closing, see the documentation
  // above PROBE_TESTS for how to hook into this little framework.
  for (let probeTest of PROBE_TESTS) {
    await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "http://example.com",
    }, async function(browser) {
      let tab = gBrowser.getTabForBrowser(browser);
      info(`Test: "${probeTest.name}"`);

      await probeTest.prepare(browser);
      // Instead of trying to fiddle with network state or
      // anything, we'll just set this attribute to fool our
      // telemetry probes into thinking the browser is in the
      // middle of loading some resources.
      tab.setAttribute("busy", true);

      histogram.clear();
      await probeTest.doAction(browser);
      let snapshot = histogram.snapshot();
      assertOnlyOneTypeSet(snapshot, probeTest.category);
    });
  }

  // The above tests used BrowserTestUtils.withNewTab, which is
  // fine for almost all categories for this probe, except for
  // "tabClosed", since withNewTab closes the tab automatically
  // before resolving, which doesn't work well for a tabClosed
  // test in the above framework. So the tabClosed tests are
  // done separately below.

  histogram.clear();
  // Now test that we can close a busy tab and get the tabClosed
  // measurement bumped.
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  // As above, instead of trying to fiddle with network state
  // or anything, we'll just set this attribute to fool our
  // telemetry probes into thinking the browser is in the
  // middle of loading some resources.
  newTab.setAttribute("busy", true);

  await BrowserTestUtils.removeTab(newTab);
  let snapshot = histogram.snapshot();
  assertOnlyOneTypeSet(snapshot, "tabClosed");
});
