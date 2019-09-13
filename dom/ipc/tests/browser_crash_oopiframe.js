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
  ok(
    SpecialPowers.useRemoteSubframes,
    "This test only makes sense of we can use OOP iframes."
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async browser => {
      let rootBC = browser.browsingContext;

      // If we load example.com in an injected subframe, we assume that this
      // will load in its own subprocess, which we can then crash.
      let iframeBC = await SpecialPowers.spawn(browser, [], async () => {
        let iframe = content.document.createElement("iframe");
        iframe.setAttribute("src", "http://example.com");

        content.document.body.appendChild(iframe);
        await ContentTaskUtils.waitForEvent(iframe, "load");
        return iframe.frameLoader.browsingContext;
      });

      is(iframeBC.parent, rootBC, "oop frame has root as parent");

      let eventFiredPromise = BrowserTestUtils.waitForEvent(
        browser,
        "oop-browser-crashed"
      );

      BrowserTestUtils.crashFrame(
        browser,
        true /* shouldShowTabCrashPage */,
        true /* shouldClearMinidumps */,
        iframeBC
      );

      info("Waiting for oop-browser-crashed event.");
      await eventFiredPromise.then(event => {
        ok(!event.isTopFrame, "should not be reporting top-level frame crash");

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

      // The BrowsingContext is re-used, but the currentWindowGlobal
      // might still be getting set up at this point. We poll to wait
      // until its created and available.
      await BrowserTestUtils.waitForCondition(() => {
        return iframeBC.currentWindowGlobal;
      });

      let newIframeURI = await SpecialPowers.spawn(iframeBC, [], async () => {
        return content.document.documentURI;
      });

      ok(
        newIframeURI.startsWith("about:framecrashed"),
        "The iframe is now pointing at about:framecrashed"
      );
    }
  );
});
