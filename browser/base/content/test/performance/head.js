"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

/**
 * Async utility function for ensuring that no unexpected uninterruptible
 * reflows occur during some period of time in a window.
 *
 * @param testFn (async function)
 *        The async function that will exercise the browser activity that is
 *        being tested for reflows.
 *
 *        The testFn will be passed a single argument, which is a frame dirtying
 *        function that can be called if the test needs to trigger frame
 *        dirtying outside of the normal mechanism.
 * @param expectedReflows (Array, optional)
 *        An Array of Objects representing reflows.
 *
 *        Example:
 *
 *        [
 *          {
 *            // This reflow is caused by lorem ipsum.
 *            // Sometimes, due to unpredictable timings, the reflow may be hit
 *            // less times, or not hit at all; in such a case a minTimes
 *            // property can be provided to avoid intermittent failures.
 *            stack: [
 *              "select@chrome://global/content/bindings/textbox.xml",
 *              "focusAndSelectUrlBar@chrome://browser/content/browser.js",
 *              "openLinkIn@chrome://browser/content/utilityOverlay.js",
 *              "openUILinkIn@chrome://browser/content/utilityOverlay.js",
 *              "BrowserOpenTab@chrome://browser/content/browser.js",
 *            ],
 *            // We expect this particular reflow to happen 2 times
 *            times: 2,
 *            // Sometimes this is not hit.
 *            minTimes: 0
 *          },
 *
 *          {
 *            // This reflow is caused by lorem ipsum. We expect this reflow
 *            // to only happen once, so we can omit the "times" property.
 *            stack: [
 *              "get_scrollPosition@chrome://global/content/bindings/scrollbox.xml",
 *              "_fillTrailingGap@chrome://browser/content/tabbrowser.xml",
 *              "_handleNewTab@chrome://browser/content/tabbrowser.xml",
 *              "onxbltransitionend@chrome://browser/content/tabbrowser.xml",
 *            ],
 *          }
 *        ]
 *
 *        Note that line numbers are not included in the stacks.
 *
 *        Order of the reflows doesn't matter. Expected reflows that aren't seen
 *        will cause an assertion failure. When this argument is not passed,
 *        it defaults to the empty Array, meaning no reflows are expected.
 * @param window (browser window, optional)
 *        The browser window to monitor. Defaults to the current window.
 */
async function withReflowObserver(testFn, expectedReflows = [], win = window) {
  let dwu = win.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils);
  let dirtyFrameFn = () => {
    try {
      dwu.ensureDirtyRootFrame();
    } catch (e) {
      // If this fails, we should probably make note of it, but it's not fatal.
      info("Note: ensureDirtyRootFrame threw an exception.");
    }
  };

  // We're going to remove the reflows one by one as we see them so that
  // we can check for expected, unseen reflows, so let's clone the array.
  // While we're at it, for reflows that omit the "times" property, default
  // it to 1.
  expectedReflows = expectedReflows.slice(0);
  expectedReflows.forEach(r => {
    r.times = r.times || 1;
  });

  let observer = {
    reflow(start, end) {
      // Gather information about the current code path, slicing out the current
      // frame.
      let path = (new Error().stack).split("\n").slice(1).map(line => {
        return line.replace(/:\d+:\d+$/, "");
      }).join("|");

      let pathWithLineNumbers = (new Error().stack).split("\n").slice(1);

      // Just in case, dirty the frame now that we've reflowed.
      dirtyFrameFn();

      // Stack trace is empty. Reflow was triggered by native code, which
      // we ignore.
      if (path === "") {
        return;
      }

      // synthesizeKey from EventUtils.js causes us to reflow. That's the test
      // harness and we don't care about that, so we'll filter that out.
      if (path.startsWith("synthesizeKey@chrome://mochikit/content/tests/SimpleTest/EventUtils.js")) {
        return;
      }

      let index = expectedReflows.findIndex(reflow => path.startsWith(reflow.stack.join("|")));

      if (index != -1) {
        Assert.ok(true, "expected uninterruptible reflow: '" +
                  JSON.stringify(pathWithLineNumbers, null, "\t") + "'");
        if (expectedReflows[index].minTimes) {
          expectedReflows[index].minTimes--;
        }
        if (--expectedReflows[index].times == 0) {
          expectedReflows.splice(index, 1);
        }
      } else {
        Assert.ok(false, "unexpected uninterruptible reflow \n" +
                         JSON.stringify(pathWithLineNumbers, null, "\t") + "\n");
      }
    },

    reflowInterruptible(start, end) {
      // We're not interested in interruptible reflows, but might as well take the
      // opportuntiy to dirty the root frame.
      dirtyFrameFn();
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                           Ci.nsISupportsWeakReference])
  };

  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell);
  docShell.addWeakReflowObserver(observer);

  Services.els.addListenerForAllEvents(win, dirtyFrameFn, true);

  try {
    dirtyFrameFn();
    await testFn(dirtyFrameFn);
  } finally {
    for (let remainder of expectedReflows) {
      if (!Number.isInteger(remainder.minTimes) || remainder.minTimes > 0) {
        Assert.ok(false,
                  `Unused expected reflow: ${JSON.stringify(remainder.stack, null, "\t")}\n` +
                  `This reflow was supposed to be hit ${remainder.minTimes || remainder.times} more time(s).\n` +
                  "This is probably a good thing - just remove it from the " +
                  "expected list.");
      }
    }

    Services.els.removeListenerForAllEvents(win, dirtyFrameFn, true);
    docShell.removeWeakReflowObserver(observer);
  }
}

async function ensureNoPreloadedBrowser() {
  // If we've got a preloaded browser, get rid of it so that it
  // doesn't interfere with the test if it's loading. We have to
  // do this before we disable preloading or changing the new tab
  // URL, otherwise _getPreloadedBrowser will return null, despite
  // the preloaded browser existing.
  let preloaded = gBrowser._getPreloadedBrowser();
  if (preloaded) {
    preloaded.remove();
  }

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtab.preload", false]],
  });

  let aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                             .getService(Ci.nsIAboutNewTabService);
  aboutNewTabService.newTabURL = "about:blank";

  registerCleanupFunction(() => {
    aboutNewTabService.resetNewTabURL();
  });
}

/**
 * Calculate and return how many additional tabs can be fit into the
 * tabstrip without causing it to overflow.
 *
 * @return int
 *         The maximum additional tabs that can be fit into the
 *         tabstrip without causing it to overflow.
 */
function computeMaxTabCount() {
  let currentTabCount = gBrowser.tabs.length;
  let newTabButton =
    document.getAnonymousElementByAttribute(gBrowser.tabContainer,
                                            "anonid", "tabs-newtab-button");
  let newTabRect = newTabButton.getBoundingClientRect();
  let tabStripRect = gBrowser.tabContainer.arrowScrollbox.getBoundingClientRect();
  let availableTabStripWidth = tabStripRect.width - newTabRect.width;

  let tabMinWidth =
    parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth, 10);

  let maxTabCount = Math.floor(availableTabStripWidth / tabMinWidth) - currentTabCount;
  Assert.ok(maxTabCount > 0,
            "Tabstrip needs to be wide enough to accomodate at least 1 more tab " +
            "without overflowing.");
  return maxTabCount;
}

/**
 * Helper function that opens up some number of about:blank tabs, and wait
 * until they're all fully open.
 *
 * @param howMany (int)
 *        How many about:blank tabs to open.
 */
async function createTabs(howMany) {
  let uris = [];
  while (howMany--) {
    uris.push("about:blank");
  }

  gBrowser.loadTabs(uris, true, false);

  await BrowserTestUtils.waitForCondition(() => {
    return Array.from(gBrowser.tabs).every(tab => tab._fullyOpen);
  });
}

/**
 * Removes all of the tabs except the originally selected
 * tab, and waits until all of the DOM nodes have been
 * completely removed from the tab strip.
 */
async function removeAllButFirstTab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.warnOnCloseOtherTabs", false]],
  });
  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
  await BrowserTestUtils.waitForCondition(() => gBrowser.tabs.length == 1);
  await SpecialPowers.popPrefEnv();
}

/**
 * Adds some entries to the Places database so that we can
 * do semi-realistic look-ups in the URL bar.
 *
 * @param searchStr (string)
 *        Optional text to add to the search history items.
 */
async function addDummyHistoryEntries(searchStr = "") {
  await PlacesUtils.history.clear();
  const NUM_VISITS = 10;
  let visits = [];

  for (let i = 0; i < NUM_VISITS; ++i) {
    visits.push({
      uri: `http://example.com/urlbar-reflows-${i}`,
      title: `Reflow test for URL bar entry #${i} - ${searchStr}`,
    });
  }

  await PlacesTestUtils.addVisits(visits);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });
}
