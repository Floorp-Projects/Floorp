"use strict";

/**
 * In this test, we crash an out-of-process iframe and
 * verify that :
 *  1. the "oop-browser-crashed" event is dispatched with
 *     the browsing context of the crashed oop subframe.
 *  2. the crashed subframe is now pointing at "about:framecrashed"
 *     page.
 */
add_task(async function() {
  // Open a new window with fission enabled.
  let win = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    fission: true,
  });

  // Wait for the provided URL to load in our browser.
  let url = "about:blank";
  let browser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);

  let rootBC = browser.browsingContext;

  // Create an oop iframe.
  let iframeID = await SpecialPowers.spawn(browser, [], async () => {
    let iframe = content.document.createElement("iframe");
    iframe.setAttribute("fission", "true");
    iframe.setAttribute("src", "http://example.com");

    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );

    content.document.body.appendChild(iframe);
    await ContentTaskUtils.waitForEvent(iframe, "load");

    let iframeBC = iframe.frameLoader.browsingContext;
    return iframeBC.id;
  });

  let iframeBC = BrowsingContext.get(iframeID);
  is(iframeBC.parent, rootBC, "oop frame has root as parent");

  BrowserTestUtils.crashFrame(
    browser,
    true /* shouldShowTabCrashPage */,
    true /* shouldClearMinidumps */,
    iframeBC
  );

  let eventFiredPromise = BrowserTestUtils.waitForEvent(
    browser,
    "oop-browser-crashed"
  );

  await eventFiredPromise.then(event => {
    isnot(
      event.browsingContextId,
      rootBC,
      "top frame browsing context id not expected."
    );

    is(
      event.browsingContextId,
      iframeBC.id,
      "oop frame browsing context id expected."
    );
  });

  info("Wait for a new browsing context to get attached to our oop iframe.");
  await BrowserTestUtils.waitForCondition(
    () => rootBC.getChildren()[0] != iframeBC
  );

  let newIframeBC = rootBC.getChildren()[0];
  let newIframeURI = await SpecialPowers.spawn(
    newIframeBC,
    [],
    () => content.document.documentURI
  );

  ok(
    newIframeURI.startsWith("about:framecrashed"),
    "The iframe is now pointing at about:framecrashed"
  );

  await BrowserTestUtils.closeWindow(win);
});
