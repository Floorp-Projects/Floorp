"use strict";

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
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
 *            // less times.
 *            stack: [
 *              "select@chrome://global/content/bindings/textbox.xml",
 *              "focusAndSelectUrlBar@chrome://browser/content/browser.js",
 *              "openLinkIn@chrome://browser/content/utilityOverlay.js",
 *              "openUILinkIn@chrome://browser/content/utilityOverlay.js",
 *              "BrowserOpenTab@chrome://browser/content/browser.js",
 *            ],
 *            // We expect this particular reflow to happen up to 2 times.
 *            maxCount: 2,
 *          },
 *
 *          {
 *            // This reflow is caused by lorem ipsum. We expect this reflow
 *            // to only happen once, so we can omit the "maxCount" property.
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

  // Collect all reflow stacks, we'll process them later.
  let reflows = [];

  let observer = {
    reflow(start, end) {
      // Gather information about the current code path.
      reflows.push(new Error().stack);

      // Just in case, dirty the frame now that we've reflowed.
      dirtyFrameFn();
    },

    reflowInterruptible(start, end) {
      // Interruptible reflows are the reflows caused by the refresh
      // driver ticking. These are fine.
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
    let knownReflows = expectedReflows.map(r => {
      return {stack: r.stack, path: r.stack.join("|"),
              count: 0, maxCount: r.maxCount || 1,
              actualStacks: new Map()};
    });
    let unexpectedReflows = new Map();
    for (let stack of reflows) {
      let path =
        stack.split("\n").slice(1) // the first frame which is our test code.
             .map(line => line.replace(/:\d+:\d+$/, "")) // strip line numbers.
             .join("|");

      // Stack trace is empty. Reflow was triggered by native code, which
      // we ignore.
      if (path === "") {
        continue;
      }

      // synthesizeKey from EventUtils.js causes us to reflow. That's the test
      // harness and we don't care about that, so we'll filter that out.
      if (path.startsWith("synthesizeKey@chrome://mochikit/content/tests/SimpleTest/EventUtils.js")) {
        continue;
      }

      let index = knownReflows.findIndex(reflow => path.startsWith(reflow.path));
      if (index != -1) {
        let reflow = knownReflows[index];
        ++reflow.count;
        reflow.actualStacks.set(stack, (reflow.actualStacks.get(stack) || 0) + 1);
      } else {
        unexpectedReflows.set(stack, (unexpectedReflows.get(stack) || 0) + 1);
      }
    }

    let formatStack = stack =>
      stack.split("\n").slice(1).map(frame => "  " + frame).join("\n");
    for (let reflow of knownReflows) {
      let firstFrame = reflow.stack[0];
      if (!reflow.count) {
        Assert.ok(false,
                  `Unused expected reflow at ${firstFrame}:\nStack:\n` +
                  reflow.stack.map(frame => "  " + frame).join("\n") + "\n" +
                  "This is probably a good thing - just remove it from the whitelist.");
      } else {
        if (reflow.count > reflow.maxCount) {
          Assert.ok(false,
                    `reflow at ${firstFrame} was encountered ${reflow.count} times,\n` +
                    `it was expected to happen up to ${reflow.maxCount} times.`);

        } else {
          todo(false, `known reflow at ${firstFrame} was encountered ${reflow.count} times`);
        }
        for (let [stack, count] of reflow.actualStacks) {
          info("Full stack" + (count > 1 ? ` (hit ${count} times)` : "") + ":\n" +
               formatStack(stack));
        }
      }
    }

    for (let [stack, count] of unexpectedReflows) {
      let location = stack.split("\n")[1].replace(/:\d+:\d+$/, "");
      Assert.ok(false,
                `unexpected reflow at ${location} hit ${count} times\n` +
                "Stack:\n" +
                formatStack(stack));
    }
    Assert.ok(!unexpectedReflows.size,
              unexpectedReflows.size + " unexpected reflows");

    Services.els.removeListenerForAllEvents(win, dirtyFrameFn, true);
    docShell.removeWeakReflowObserver(observer);
  }
}

async function ensureNoPreloadedBrowser(win = window) {
  // If we've got a preloaded browser, get rid of it so that it
  // doesn't interfere with the test if it's loading. We have to
  // do this before we disable preloading or changing the new tab
  // URL, otherwise _getPreloadedBrowser will return null, despite
  // the preloaded browser existing.
  let preloaded = win.gBrowser._getPreloadedBrowser();
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
 * The navigation toolbar is overflowable, meaning that some items
 * will be moved and held within a sub-panel if the window gets too
 * small to show their icons. The calculation for hiding those items
 * occurs after resize events, and is debounced using a DeferredTask.
 * This utility function allows us to fast-forward to just running
 * that function for that DeferredTask instead of waiting for the
 * debounce timeout to occur.
 */
function forceImmediateToolbarOverflowHandling(win) {
  let overflowableToolbar = win.document.getElementById("nav-bar").overflowable;
  if (overflowableToolbar._lazyResizeHandler && overflowableToolbar._lazyResizeHandler.isArmed) {
    overflowableToolbar._lazyResizeHandler.disarm();
    // Ensure the root frame is dirty before resize so that, if we're
    // in the middle of a reflow test, we record the reflows deterministically.
    let dwu = win.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils);
    dwu.ensureDirtyRootFrame();
    overflowableToolbar._onLazyResize();
  }
}

async function prepareSettledWindow() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await ensureNoPreloadedBrowser(win);
  forceImmediateToolbarOverflowHandling(win);
  return win;
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

function compareFrames(frame, previousFrame) {
  // Accessing the Math global is expensive as the test executes in a
  // non-syntactic scope. Accessing it as a lexical variable is enough
  // to make the code JIT well.
  const M = Math;

  function expandRect(x, y, rect) {
    if (rect.x2 < x)
      rect.x2 = x;
    else if (rect.x1 > x)
      rect.x1 = x;
    if (rect.y2 < y)
      rect.y2 = y;
  }

  function isInRect(x, y, rect) {
    return (rect.y2 == y || rect.y2 == y - 1) && rect.x1 - 1 <= x && x <= rect.x2 + 1;
  }

  if (frame.height != previousFrame.height ||
      frame.width != previousFrame.width) {
    // If the frames have different sizes, assume the whole window has
    // been repainted when the window was resized.
    return [{x1: 0, x2: frame.width, y1: 0, y2: frame.height}];
  }

  let l = frame.data.length;
  let different = [];
  let rects = [];
  for (let i = 0; i < l; i += 4) {
    let x = (i / 4) % frame.width;
    let y = M.floor((i / 4) / frame.width);
    for (let j = 0; j < 4; ++j) {
      let index = i + j;

      if (frame.data[index] != previousFrame.data[index]) {
        let found = false;
        for (let rect of rects) {
          if (isInRect(x, y, rect)) {
            expandRect(x, y, rect);
            found = true;
            break;
          }
        }
        if (!found)
          rects.unshift({x1: x, x2: x, y1: y, y2: y});

        different.push(i);
        break;
      }
    }
  }
  rects.reverse();

  // The following code block merges rects that are close to each other
  // (less than maxEmptyPixels away).
  // This is needed to avoid having a rect for each letter when a label moves.
  const maxEmptyPixels = 3;
  let areRectsContiguous = function(r1, r2) {
    return r1.y2 >= r2.y1 - 1 - maxEmptyPixels &&
           r2.x1 - 1 - maxEmptyPixels <= r1.x2 &&
           r2.x2 >= r1.x1 - 1 - maxEmptyPixels;
  };
  let hasMergedRects;
  do {
    hasMergedRects = false;
    for (let r = rects.length - 1; r > 0; --r) {
      let rr = rects[r];
      for (let s = r - 1; s >= 0; --s) {
        let rs = rects[s];
        if (areRectsContiguous(rs, rr)) {
          rs.x1 = Math.min(rs.x1, rr.x1);
          rs.y1 = Math.min(rs.y1, rr.y1);
          rs.x2 = Math.max(rs.x2, rr.x2);
          rs.y2 = Math.max(rs.y2, rr.y2);
          rects.splice(r, 1);
          hasMergedRects = true;
          break;
        }
      }
    }
  } while (hasMergedRects);

  // For convenience, pre-compute the width and height of each rect.
  rects.forEach(r => {
    r.w = r.x2 - r.x1;
    r.h = r.y2 - r.y1;
  });

  return rects;
}

function dumpFrame({data, width, height}) {
  let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.mozOpaque = true;
  canvas.width = width;
  canvas.height = height;

  canvas.getContext("2d", {alpha: false, willReadFrequently: true})
        .putImageData(new ImageData(data, width, height), 0, 0);

  info(canvas.toDataURL());
}
