"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

/**
 * This function can be called if the test needs to trigger frame dirtying
 * outside of the normal mechanism.
 *
 * @param win (dom window)
 *        The window in which the frame tree needs to be marked as dirty.
 */
function dirtyFrame(win) {
  let dwu = win.windowUtils;
  try {
    dwu.ensureDirtyRootFrame();
  } catch (e) {
    // If this fails, we should probably make note of it, but it's not fatal.
    info("Note: ensureDirtyRootFrame threw an exception:" + e);
  }
}

/**
 * Async utility function to collect the stacks of uninterruptible reflows
 * occuring during some period of time in a window.
 *
 * @param testPromise (Promise)
 *        A promise that is resolved when the data collection should stop.
 *
 * @param win (browser window, optional)
 *        The browser window to monitor. Defaults to the current window.
 *
 * @return An array of reflow stacks
 */
async function recordReflows(testPromise, win = window) {
  // Collect all reflow stacks, we'll process them later.
  let reflows = [];

  let observer = {
    reflow(start, end) {
      // Gather information about the current code path.
      reflows.push(new Error().stack);

      // Just in case, dirty the frame now that we've reflowed.
      dirtyFrame(win);
    },

    reflowInterruptible(start, end) {
      // Interruptible reflows are the reflows caused by the refresh
      // driver ticking. These are fine.
    },

    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIReflowObserver,
      Ci.nsISupportsWeakReference,
    ]),
  };

  let docShell = win.docShell;
  docShell.addWeakReflowObserver(observer);

  let dirtyFrameFn = event => {
    if (event.type != "MozAfterPaint") {
      dirtyFrame(win);
    }
  };
  Services.els.addListenerForAllEvents(win, dirtyFrameFn, true);

  try {
    dirtyFrame(win);
    await testPromise;
  } finally {
    Services.els.removeListenerForAllEvents(win, dirtyFrameFn, true);
    docShell.removeWeakReflowObserver(observer);
  }

  return reflows;
}

/**
 * Utility function to report unexpected reflows.
 *
 * @param reflows (Array)
 *        An array of reflow stacks returned by recordReflows.
 *
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
 */
function reportUnexpectedReflows(reflows, expectedReflows = []) {
  let knownReflows = expectedReflows.map(r => {
    return {
      stack: r.stack,
      path: r.stack.join("|"),
      count: 0,
      maxCount: r.maxCount || 1,
      actualStacks: new Map(),
    };
  });
  let unexpectedReflows = new Map();

  if (knownReflows.some(r => r.path.includes("*"))) {
    Assert.ok(
      false,
      "Do not include async frames in the stack, as " +
        "that feature is not available on all trees."
    );
  }

  for (let stack of reflows) {
    let path = stack
      .split("\n")
      .slice(1) // the first frame which is our test code.
      .map(line => line.replace(/:\d+:\d+$/, "")) // strip line numbers.
      .join("|");

    // Stack trace is empty. Reflow was triggered by native code, which
    // we ignore.
    if (path === "") {
      continue;
    }

    // Functions from EventUtils.js calculate coordinates and
    // dimensions, causing us to reflow. That's the test
    // harness and we don't care about that, so we'll filter that out.
    if (
      /^(synthesize|send|createDragEventObject).*?@chrome:\/\/mochikit.*?EventUtils\.js/.test(
        path
      )
    ) {
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
    stack
      .split("\n")
      .slice(1)
      .map(frame => "  " + frame)
      .join("\n");
  for (let reflow of knownReflows) {
    let firstFrame = reflow.stack[0];
    if (!reflow.count) {
      Assert.ok(
        false,
        `Unused expected reflow at ${firstFrame}:\nStack:\n` +
          reflow.stack.map(frame => "  " + frame).join("\n") +
          "\n" +
          "This is probably a good thing - just remove it from the whitelist."
      );
    } else {
      if (reflow.count > reflow.maxCount) {
        Assert.ok(
          false,
          `reflow at ${firstFrame} was encountered ${reflow.count} times,\n` +
            `it was expected to happen up to ${reflow.maxCount} times.`
        );
      } else {
        todo(
          false,
          `known reflow at ${firstFrame} was encountered ${reflow.count} times`
        );
      }
      for (let [stack, count] of reflow.actualStacks) {
        info(
          "Full stack" +
            (count > 1 ? ` (hit ${count} times)` : "") +
            ":\n" +
            formatStack(stack)
        );
      }
    }
  }

  for (let [stack, count] of unexpectedReflows) {
    let location = stack.split("\n")[1].replace(/:\d+:\d+$/, "");
    Assert.ok(
      false,
      `unexpected reflow at ${location} hit ${count} times\n` +
        "Stack:\n" +
        formatStack(stack)
    );
  }
  Assert.ok(
    !unexpectedReflows.size,
    unexpectedReflows.size + " unexpected reflows"
  );
}

async function ensureNoPreloadedBrowser(win = window) {
  // If we've got a preloaded browser, get rid of it so that it
  // doesn't interfere with the test if it's loading. We have to
  // do this before we disable preloading or changing the new tab
  // URL, otherwise _getPreloadedBrowser will return null, despite
  // the preloaded browser existing.
  NewTabPagePreloading.removePreloadedBrowser(win);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtab.preload", false]],
  });

  let aboutNewTabService = Cc[
    "@mozilla.org/browser/aboutnewtab-service;1"
  ].getService(Ci.nsIAboutNewTabService);
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
  if (
    overflowableToolbar._lazyResizeHandler &&
    overflowableToolbar._lazyResizeHandler.isArmed
  ) {
    overflowableToolbar._lazyResizeHandler.disarm();
    // Ensure the root frame is dirty before resize so that, if we're
    // in the middle of a reflow test, we record the reflows deterministically.
    let dwu = win.windowUtils;
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

// Use this function to avoid catching a reflow related to calling focus on the
// urlbar and changed rects for its dropmarker when opening new tabs.
async function ensureFocusedUrlbar() {
  // The switchingtabs attribute prevents the historydropmarker opacity
  // transition, so if we expect a transitionend event when this attribute
  // is set, we wait forever. (it's removed off a MozAfterPaint event listener)
  await BrowserTestUtils.waitForCondition(
    () => !gURLBar.hasAttribute("switchingtabs")
  );

  let opacityPromise = BrowserTestUtils.waitForEvent(
    gURLBar.dropmarker,
    "transitionend",
    false,
    e => e.propertyName === "opacity"
  );
  gURLBar.focus();
  await opacityPromise;
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
  let newTabButton = gBrowser.tabContainer.newTabButton;
  let newTabRect = newTabButton.getBoundingClientRect();
  let tabStripRect = gBrowser.tabContainer.arrowScrollbox.getBoundingClientRect();
  let availableTabStripWidth = tabStripRect.width - newTabRect.width;

  let tabMinWidth = parseInt(
    getComputedStyle(gBrowser.selectedTab, null).minWidth,
    10
  );

  let maxTabCount =
    Math.floor(availableTabStripWidth / tabMinWidth) - currentTabCount;
  Assert.ok(
    maxTabCount > 0,
    "Tabstrip needs to be wide enough to accomodate at least 1 more tab " +
      "without overflowing."
  );
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

  gBrowser.loadTabs(uris, {
    inBackground: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

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

/**
 * Async utility function to capture a screenshot of each painted frame.
 *
 * @param testPromise (Promise)
 *        A promise that is resolved when the data collection should stop.
 *
 * @param win (browser window, optional)
 *        The browser window to monitor. Defaults to the current window.
 *
 * @return An array of screenshots
 */
async function recordFrames(testPromise, win = window) {
  let canvas = win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  canvas.mozOpaque = true;
  let ctx = canvas.getContext("2d", { alpha: false, willReadFrequently: true });

  let frames = [];

  let afterPaintListener = event => {
    let width, height;
    canvas.width = width = win.innerWidth;
    canvas.height = height = win.innerHeight;
    ctx.drawWindow(
      win,
      0,
      0,
      width,
      height,
      "white",
      ctx.DRAWWINDOW_DO_NOT_FLUSH |
        ctx.DRAWWINDOW_DRAW_VIEW |
        ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES |
        ctx.DRAWWINDOW_USE_WIDGET_LAYERS
    );
    let data = Cu.cloneInto(ctx.getImageData(0, 0, width, height).data, {});
    if (frames.length) {
      // Compare this frame with the previous one to avoid storing duplicate
      // frames and running out of memory.
      let previous = frames[frames.length - 1];
      if (previous.width == width && previous.height == height) {
        let equals = true;
        for (let i = 0; i < data.length; ++i) {
          if (data[i] != previous.data[i]) {
            equals = false;
            break;
          }
        }
        if (equals) {
          return;
        }
      }
    }
    frames.push({ data, width, height });
  };
  win.addEventListener("MozAfterPaint", afterPaintListener);

  // If the test is using an existing window, capture a frame immediately.
  if (win.document.readyState == "complete") {
    afterPaintListener();
  }

  try {
    await testPromise;
  } finally {
    win.removeEventListener("MozAfterPaint", afterPaintListener);
  }

  return frames;
}

// How many identical pixels to accept between 2 rects when deciding to merge
// them.
const kMaxEmptyPixels = 3;
function compareFrames(frame, previousFrame) {
  // Accessing the Math global is expensive as the test executes in a
  // non-syntactic scope. Accessing it as a lexical variable is enough
  // to make the code JIT well.
  const M = Math;

  function expandRect(x, y, rect) {
    if (rect.x2 < x) {
      rect.x2 = x;
    } else if (rect.x1 > x) {
      rect.x1 = x;
    }
    if (rect.y2 < y) {
      rect.y2 = y;
    }
  }

  function isInRect(x, y, rect) {
    return (
      (rect.y2 == y || rect.y2 == y - 1) && rect.x1 - 1 <= x && x <= rect.x2 + 1
    );
  }

  if (
    frame.height != previousFrame.height ||
    frame.width != previousFrame.width
  ) {
    // If the frames have different sizes, assume the whole window has
    // been repainted when the window was resized.
    return [{ x1: 0, x2: frame.width, y1: 0, y2: frame.height }];
  }

  let l = frame.data.length;
  let different = [];
  let rects = [];
  for (let i = 0; i < l; i += 4) {
    let x = (i / 4) % frame.width;
    let y = M.floor(i / 4 / frame.width);
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
        if (!found) {
          rects.unshift({ x1: x, x2: x, y1: y, y2: y });
        }

        different.push(i);
        break;
      }
    }
  }
  rects.reverse();

  // The following code block merges rects that are close to each other
  // (less than kMaxEmptyPixels away).
  // This is needed to avoid having a rect for each letter when a label moves.
  let areRectsContiguous = function(r1, r2) {
    return (
      r1.y2 >= r2.y1 - 1 - kMaxEmptyPixels &&
      r2.x1 - 1 - kMaxEmptyPixels <= r1.x2 &&
      r2.x2 >= r1.x1 - 1 - kMaxEmptyPixels
    );
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
    r.w = r.x2 - r.x1 + 1;
    r.h = r.y2 - r.y1 + 1;
  });

  return rects;
}

function dumpFrame({ data, width, height }) {
  let canvas = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  canvas.mozOpaque = true;
  canvas.width = width;
  canvas.height = height;

  canvas
    .getContext("2d", { alpha: false, willReadFrequently: true })
    .putImageData(new ImageData(data, width, height), 0, 0);

  info(canvas.toDataURL());
}

/**
 * Utility function to report unexpected changed areas on screen.
 *
 * @param frames (Array)
 *        An array of frames captured by recordFrames.
 *
 * @param expectations (Object)
 *        An Object indicating which changes on screen are expected.
 *        If can contain the following optional fields:
 *         - filter: a function used to exclude changed rects that are expected.
 *            It takes the following parameters:
 *             - rects: an array of changed rects
 *             - frame: the current frame
 *             - previousFrame: the previous frame
 *            It returns an array of rects. This array is typically a copy of
 *            the rects parameter, from which identified expected changes have
 *            been excluded.
 *         - exceptions: an array of objects describing known flicker bugs.
 *           Example:
 *             exceptions: [
 *               {name: "bug 1nnnnnn - the foo icon shouldn't flicker",
 *                condition: r => r.w == 14 && r.y1 == 0 && ... }
 *               },
 *               {name: "bug ...
 *             ]
 */
function reportUnexpectedFlicker(frames, expectations) {
  info("comparing " + frames.length + " frames");

  let unexpectedRects = 0;
  for (let i = 1; i < frames.length; ++i) {
    let frame = frames[i],
      previousFrame = frames[i - 1];
    let rects = compareFrames(frame, previousFrame);

    if (expectations.filter) {
      rects = expectations.filter(rects, frame, previousFrame);
    }

    rects = rects.filter(rect => {
      let rectText = `${rect.toSource()}, window width: ${frame.width}`;
      for (let e of expectations.exceptions || []) {
        if (e.condition(rect)) {
          todo(false, e.name + ", " + rectText);
          return false;
        }
      }

      ok(false, "unexpected changed rect: " + rectText);
      return true;
    });

    if (!rects.length) {
      continue;
    }

    // Before dumping a frame with unexpected differences for the first time,
    // ensure at least one previous frame has been logged so that it's possible
    // to see the differences when examining the log.
    if (!unexpectedRects) {
      dumpFrame(previousFrame);
    }
    unexpectedRects += rects.length;
    dumpFrame(frame);
  }
  is(unexpectedRects, 0, "should have 0 unknown flickering areas");
}

/**
 * This is the main function that performance tests in this folder will call.
 *
 * The general idea is that individual tests provide a test function (testFn)
 * that will perform some user interactions we care about (eg. open a tab), and
 * this withPerfObserver function takes care of setting up and removing the
 * observers and listener we need to detect common performance issues.
 *
 * Once testFn is done, withPerfObserver will analyse the collected data and
 * report anything unexpected.
 *
 * @param testFn (async function)
 *        An async function that exercises some part of the browser UI.
 *
 * @param exceptions (object, optional)
 *        An Array of Objects representing expectations and known issues.
 *        It can contain the following fields:
 *         - expectedReflows: an array of expected reflow stacks.
 *           (see the comment above reportUnexpectedReflows for an example)
 *         - frames: an object setting expectations for what will change
 *           on screen during the test, and the known flicker bugs.
 *           (see the comment above reportUnexpectedFlicker for an example)
 */
async function withPerfObserver(testFn, exceptions = {}, win = window) {
  let resolveFn, rejectFn;
  let promiseTestDone = new Promise((resolve, reject) => {
    resolveFn = resolve;
    rejectFn = reject;
  });

  let promiseReflows = recordReflows(promiseTestDone, win);
  let promiseFrames = recordFrames(promiseTestDone, win);

  testFn().then(resolveFn, rejectFn);
  await promiseTestDone;

  let reflows = await promiseReflows;
  reportUnexpectedReflows(reflows, exceptions.expectedReflows);

  let frames = await promiseFrames;
  reportUnexpectedFlicker(frames, exceptions.frames);
}

/**
 * This test ensures that there are no unexpected
 * uninterruptible reflows when typing into the URL bar
 * with the default values in Places.
 *
 * @param {bool} keyed
 *        Pass true to synthesize typing the search string one key at a time.
 * @param {array} expectedReflowsFirstOpen
 *        The array of expected reflow stacks when the panel is first opened.
 * @param {array} [expectedReflowsSecondOpen]
 *        The array of expected reflow stacks when the panel is subsequently
 *        opened, if you're testing opening the panel twice.
 */
async function runUrlbarTest(
  keyed,
  expectedReflowsFirstOpen,
  expectedReflowsSecondOpen = null
) {
  const SEARCH_TERM = keyed ? "" : "urlbar-reflows-" + Date.now();
  await addDummyHistoryEntries(SEARCH_TERM);

  let win = await prepareSettledWindow();

  let URLBar = win.gURLBar;

  URLBar.focus();
  URLBar.value = SEARCH_TERM;
  let testFn = async function() {
    let popup = URLBar.view;
    let oldOnQueryResults = popup.onQueryResults.bind(popup);
    let oldOnQueryFinished = popup.onQueryFinished.bind(popup);

    // We need to invalidate the frame tree outside of the normal
    // mechanism since invalidations and result additions to the
    // URL bar occur without firing JS events (which is how we
    // normally know to dirty the frame tree).
    popup.onQueryResults = context => {
      dirtyFrame(win);
      oldOnQueryResults(context);
    };

    popup.onQueryFinished = context => {
      dirtyFrame(win);
      oldOnQueryFinished(context);
    };

    let waitExtra = async () => {
      // There are several setTimeout(fn, 0); calls inside autocomplete.xml
      // that we need to wait for. Since those have higher priority than
      // idle callbacks, we can be sure they will have run once this
      // idle callback is called. The timeout seems to be required in
      // automation - presumably because the machines can be pretty busy
      // especially if it's GC'ing from previous tests.
      await new Promise(resolve =>
        win.requestIdleCallback(resolve, { timeout: 1000 })
      );
    };

    if (keyed) {
      // Only keying in 6 characters because the number of reflows triggered
      // is so high that we risk timing out the test if we key in any more.
      let searchTerm = "ows-10";
      for (let i = 0; i < searchTerm.length; ++i) {
        let char = searchTerm[i];
        EventUtils.synthesizeKey(char, {}, win);
        await UrlbarTestUtils.promiseSearchComplete(win);
        await waitExtra();
      }
    } else {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window: win,
        waitForFocus: SimpleTest.waitForFocus,
        value: URLBar.value,
      });
      await waitExtra();
    }

    await UrlbarTestUtils.promisePopupClose(win);
  };

  let dropmarkerRect = URLBar.dropmarker.getBoundingClientRect();
  let textBoxRect = URLBar.querySelector(
    "moz-input-box"
  ).getBoundingClientRect();
  let resultsRect = {
    top: URLBar.textbox.closest("toolbar").getBoundingClientRect().bottom,
    right: win.innerWidth,
    bottom: win.innerHeight,
    left: 0,
  };
  let expectedRects = {
    filter: rects =>
      rects.filter(
        r =>
          !// We put text into the urlbar so expect its textbox to change.
          (
            (r.x1 >= textBoxRect.left &&
              r.x2 <= textBoxRect.right &&
              r.y1 >= textBoxRect.top &&
              r.y2 <= textBoxRect.bottom) ||
            // The dropmarker is displayed as active during some of the test.
            // dropmarkerRect.left isn't always an integer, hence the - 1 and + 1
            (r.x1 >= dropmarkerRect.left - 1 &&
              r.x2 <= dropmarkerRect.right + 1 &&
              r.y1 >= dropmarkerRect.top &&
              r.y2 <= dropmarkerRect.bottom) ||
            // We expect many changes in the results view.
            (r.x1 >= resultsRect.left &&
              r.x2 <= resultsRect.right &&
              r.y1 >= resultsRect.top &&
              r.y2 <= resultsRect.bottom)
          )
      ),
  };

  info("First opening");
  await withPerfObserver(
    testFn,
    { expectedReflows: expectedReflowsFirstOpen, frames: expectedRects },
    win
  );

  if (expectedReflowsSecondOpen) {
    info("Second opening");
    await withPerfObserver(
      testFn,
      { expectedReflows: expectedReflowsSecondOpen, frames: expectedRects },
      win
    );
  }

  await BrowserTestUtils.closeWindow(win);
}
