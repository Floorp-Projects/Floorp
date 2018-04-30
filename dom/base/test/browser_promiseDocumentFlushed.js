/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Dirties style and layout on the current browser window.
 *
 * @param {Number} Optional factor by which to modify the DOM. Useful for
 *        when multiple calls to dirtyTheDOM may occur, and you need them
 *        to dirty the DOM differently from one another. If you only need
 *        to dirty the DOM once, this can be omitted.
 */
function dirtyStyleAndLayout(factor = 1) {
  gNavToolbox.style.padding = factor + "px";
}

/**
 * Dirties style of the current browser window, but NOT layout.
 */
function dirtyStyle() {
  gNavToolbox.style.color = "red";
}

const gWindowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);

/**
 * Asserts that no style or layout flushes are required by the
 * current window.
 */
function assertNoFlushesRequired() {
  Assert.ok(!gWindowUtils.needsFlush(Ci.nsIDOMWindowUtils.FLUSH_STYLE),
            "No style flushes are required.");
  Assert.ok(!gWindowUtils.needsFlush(Ci.nsIDOMWindowUtils.FLUSH_LAYOUT),
            "No layout flushes are required.");
}

/**
 * Asserts that the DOM has been dirtied, and so style and layout flushes
 * are required.
 */
function assertFlushesRequired() {
  Assert.ok(gWindowUtils.needsFlush(Ci.nsIDOMWindowUtils.FLUSH_STYLE),
            "Style flush required.");
  Assert.ok(gWindowUtils.needsFlush(Ci.nsIDOMWindowUtils.FLUSH_LAYOUT),
            "Layout flush required.");
}

/**
 * Removes style changes from dirtyTheDOM() from the browser window,
 * and resolves once the refresh driver ticks.
 */
async function cleanTheDOM() {
  gNavToolbox.style.padding = "";
  gNavToolbox.style.color = "";
  await window.promiseDocumentFlushed(() => {});
}

add_task(async function setup() {
  registerCleanupFunction(cleanTheDOM);
});

/**
 * Tests that if the DOM is dirty, that promiseDocumentFlushed will
 * resolve once layout and style have been flushed.
 */
add_task(async function test_basic() {
  dirtyStyleAndLayout();
  assertFlushesRequired();

  await window.promiseDocumentFlushed(() => {});
  assertNoFlushesRequired();

  dirtyStyle();
  assertFlushesRequired();

  await window.promiseDocumentFlushed(() => {});
  assertNoFlushesRequired();

  // The DOM should be clean already, but we'll do this anyway to isolate
  // failures from other tests.
  await cleanTheDOM();
});

/**
 * Test that values returned by the callback passed to promiseDocumentFlushed
 * get passed down through the Promise resolution.
 */
add_task(async function test_can_get_results_from_callback() {
  const NEW_PADDING = "2px";

  gNavToolbox.style.padding = NEW_PADDING;

  assertFlushesRequired();

  let paddings = await window.promiseDocumentFlushed(() => {
    let style = window.getComputedStyle(gNavToolbox);
    return {
      left: style.paddingLeft,
      right: style.paddingRight,
      top: style.paddingTop,
      bottom: style.paddingBottom,
    };
  });

  for (let prop in paddings) {
    Assert.equal(paddings[prop], NEW_PADDING,
                 "Got expected padding");
  }

  await cleanTheDOM();

  gNavToolbox.style.padding = NEW_PADDING;

  assertFlushesRequired();

  let rect = await window.promiseDocumentFlushed(() => {
    let observer = {
      reflow() {
        Assert.ok(false, "A reflow should not have occurred.");
      },
      reflowInterruptible() {},
      QueryInterface: ChromeUtils.generateQI([Ci.nsIReflowObserver,
                                              Ci.nsISupportsWeakReference])
    };

    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    docShell.addWeakReflowObserver(observer);

    let toolboxRect = gNavToolbox.getBoundingClientRect();

    docShell.removeWeakReflowObserver(observer);
    return toolboxRect;
  });

  // The actual values of this rect aren't super important for
  // the purposes of this test - we just want to know that a valid
  // rect was returned, so checking for properties being greater than
  // 0 is sufficient.
  for (let property of ["width", "height"]) {
    Assert.ok(rect[property] > 0, `Rect property ${property} > 0 (${rect[property]})`);
  }

  await cleanTheDOM();
});

/**
 * Test that if promiseDocumentFlushed is requested on a window
 * that closes before it gets a chance to do a refresh driver
 * tick, the promiseDocumentFlushed Promise is still resolved, and
 * the callback is still called.
 */
add_task(async function test_resolved_in_window_close() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await win.promiseDocumentFlushed(() => {});

  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDocShell);
  docShell.contentViewer.pausePainting();

  win.gNavToolbox.style.padding = "5px";

  const EXPECTED = 1234;
  let promise = win.promiseDocumentFlushed(() => {
    // Despite the window not painting before closing, this
    // callback should be fired when the window gets torn
    // down and should stil be able to return a result.
    return EXPECTED;
  });

  await BrowserTestUtils.closeWindow(win);
  Assert.equal(await promise, EXPECTED);
});

/**
 * Test that re-entering promiseDocumentFlushed is not possible
 * from within a promiseDocumentFlushed callback. Doing so will
 * result in the outer Promise rejecting with NS_ERROR_FAILURE.
 */
add_task(async function test_reentrancy() {
  dirtyStyleAndLayout();
  assertFlushesRequired();

  let promise = window.promiseDocumentFlushed(() => {
    return window.promiseDocumentFlushed(() => {
      Assert.ok(false, "Should never run this.");
    });
  });

  await Assert.rejects(promise, ex => ex.result == Cr.NS_ERROR_FAILURE);
});

/**
 * Tests the expected execution order of a series of promiseDocumentFlushed
 * calls, their callbacks, and the resolutions of their Promises.
 *
 * When multiple promiseDocumentFlushed callbacks are queued, the callbacks
 * should always been run first before any of the Promises are resolved.
 *
 * The callbacks should run in the order that they were queued in via
 * promiseDocumentFlushed. The Promise resolutions should similarly run
 * in the order that promiseDocumentFlushed was called in.
 */
add_task(async function test_execution_order() {
  let result = [];

  dirtyStyleAndLayout(1);
  let promise1 = window.promiseDocumentFlushed(() => {
    result.push(0);
  }).then(() => {
    result.push(2);
  });

  let promise2 = window.promiseDocumentFlushed(() => {
    result.push(1);
  }).then(() => {
    result.push(3);
  });

  await Promise.all([promise1, promise2]);

  Assert.equal(result.length, 4,
    "Should have run all callbacks and Promises.");

  let promise3 = window.promiseDocumentFlushed(() => {
    result.push(4);
  }).then(() => {
    result.push(6);
  });

  let promise4 = window.promiseDocumentFlushed(() => {
    result.push(5);
  }).then(() => {
    result.push(7);
  });

  await Promise.all([promise3, promise4]);

  Assert.equal(result.length, 8,
    "Should have run all callbacks and Promises.");

  for (let i = 0; i < result.length; ++i) {
    Assert.equal(result[i], i,
      "Callbacks and Promises should have run in the expected order.");
  }
});
