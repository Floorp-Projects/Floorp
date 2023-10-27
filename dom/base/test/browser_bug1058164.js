/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

SpecialPowers.pushPrefEnv({
  set: [["security.allow_eval_with_system_principal", true]],
});

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

let gListenerId = 0;

/**
 * Adds a content event listener on the given browser element. NOTE: this test
 * is checking the behavior of pageshow and pagehide as seen by frame scripts,
 * so it is specifically implemented using the message message manager.
 * Similar to BrowserTestUtils.waitForContentEvent, but the listener will fire
 * until it is removed. A callable object is returned that, when called, removes
 * the event listener. Note that this function works even if the browser's
 * frameloader is swapped.
 *
 * @param {xul:browser} browser
 *        The browser element to listen for events in.
 * @param {string} eventName
 *        Name of the event to listen to.
 * @param {function} listener
 *        Function to call in parent process when event fires.
 *        Not passed any arguments.
 * @param {function} checkFn [optional]
 *        Called with the Event object as argument, should return true if the
 *        event is the expected one, or false if it should be ignored and
 *        listening should continue. If not specified, the first event with
 *        the specified name resolves the returned promise. This is called
 *        within the content process and can have no closure environment.
 *
 * @returns function
 *        If called, the return value will remove the event listener.
 */
function addContentEventListenerWithMessageManager(
  browser,
  eventName,
  listener,
  checkFn
) {
  let id = gListenerId++;
  let checkFnSource = checkFn
    ? encodeURIComponent(escape(checkFn.toSource()))
    : "";

  // To correctly handle frameloader swaps, we load a frame script
  // into all tabs but ignore messages from the ones not related to
  // |browser|.

  /* eslint-disable no-eval */
  function frameScript(innerId, innerEventName, innerCheckFnSource) {
    let innerCheckFn;
    if (innerCheckFnSource) {
      innerCheckFn = eval(`(() => (${unescape(innerCheckFnSource)}))()`);
    }

    function innerListener(event) {
      if (innerCheckFn && !innerCheckFn(event)) {
        return;
      }
      sendAsyncMessage("ContentEventListener:Run", innerId);
    }
    function removeListener(msg) {
      if (msg.data == innerId) {
        removeMessageListener("ContentEventListener:Remove", removeListener);
        removeEventListener(innerEventName, innerListener);
      }
    }
    addMessageListener("ContentEventListener:Remove", removeListener);
    addEventListener(innerEventName, innerListener);
  }
  /* eslint-enable no-eval */

  let frameScriptSource = `data:,(${frameScript.toString()})(${id}, "${eventName}",
     "${checkFnSource}")`;

  let mm = Services.mm;

  function runListener(msg) {
    if (msg.data == id && msg.target == browser) {
      listener();
    }
  }
  mm.addMessageListener("ContentEventListener:Run", runListener);

  let needCleanup = true;

  let unregisterFunction = function () {
    if (!needCleanup) {
      return;
    }
    needCleanup = false;
    mm.removeMessageListener("ContentEventListener:Run", runListener);
    mm.broadcastAsyncMessage("ContentEventListener:Remove", id);
    mm.removeDelayedFrameScript(frameScriptSource);
  };

  function cleanupObserver(subject, topic, data) {
    if (subject == browser.messageManager) {
      unregisterFunction();
    }
  }

  mm.loadFrameScript(frameScriptSource, true);

  return unregisterFunction;
}

/**
 * Returns a Promise that resolves when it sees a pageshow and
 * pagehide events in a particular order, where each event must
 * have the persisted property set to true. Will cause a test
 * failure to be logged if it sees an event out of order.
 *
 * @param browser (<xul:browser>)
 *        The browser to expect the events from.
 * @param expectedOrder (array)
 *        An array of strings describing what order the pageshow
 *        and pagehide events should be in.
 *        Example:
 *        ["pageshow", "pagehide", "pagehide", "pageshow"]
 * @returns Promise
 */
function prepareForVisibilityEvents(browser, expectedOrder) {
  return new Promise(resolve => {
    let order = [];

    let rmvHide, rmvShow;

    let checkSatisfied = () => {
      if (order.length < expectedOrder.length) {
        // We're still waiting...
        return;
      }
      rmvHide();
      rmvShow();

      for (let i = 0; i < expectedOrder.length; ++i) {
        is(order[i], expectedOrder[i], "Got expected event");
      }
      resolve();
    };

    let eventListener = type => {
      order.push(type);
      checkSatisfied();
    };

    let checkFn = e => e.persisted;

    rmvHide = addContentEventListenerWithMessageManager(
      browser,
      "pagehide",
      () => eventListener("pagehide"),
      checkFn
    );
    rmvShow = addContentEventListenerWithMessageManager(
      browser,
      "pageshow",
      () => eventListener("pageshow"),
      checkFn
    );
  });
}

/**
 * Tests that frame scripts get pageshow / pagehide events when
 * swapping browser frameloaders (which occurs when moving a tab
 * into a different window).
 */
add_task(async function test_swap_frameloader_pagevisibility_events() {
  // Disable window occlusion. Bug 1733955
  if (navigator.platform.indexOf("Win") == 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["widget.windows.window_occlusion_tracking.enabled", false]],
    });
  }

  // Load a new tab that we'll tear out...
  let tab = BrowserTestUtils.addTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;
  let firstBrowser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(firstBrowser);

  // Swap the browser out to a new window
  let newWindow = gBrowser.replaceTabWithWindow(tab);

  // We have to wait for the window to load so we can get the selected browser
  // to listen to.
  await BrowserTestUtils.waitForEvent(newWindow, "DOMContentLoaded");
  let newWindowBrowser = newWindow.gBrowser.selectedBrowser;

  // Wait for the expected pagehide and pageshow events on the initial browser
  await prepareForVisibilityEvents(newWindowBrowser, ["pagehide", "pageshow"]);

  // Now let's send the browser back to the original window

  // First, create a new, empty browser tab to replace the window with
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  let emptyBrowser = newTab.linkedBrowser;

  // Wait for that initial doc to be visible because if its pageshow hasn't
  // happened we don't confuse it with the other expected events.
  await ContentTask.spawn(emptyBrowser, null, async () => {
    if (content.document.visibilityState === "hidden") {
      info("waiting for hidden emptyBrowser to be visible");
      await ContentTaskUtils.waitForEvent(
        content.document,
        "visibilitychange",
        {}
      );
    }
  });

  info("emptyBrowser is shown now.");

  // The empty tab we just added show now fire a pagehide as its replaced,
  // and a pageshow once the swap is finished.
  let emptyBrowserPromise = prepareForVisibilityEvents(emptyBrowser, [
    "pagehide",
    "pageshow",
  ]);

  gBrowser.swapBrowsersAndCloseOther(newTab, newWindow.gBrowser.selectedTab);

  await emptyBrowserPromise;

  gBrowser.removeTab(gBrowser.selectedTab);
});
