"use strict";

/*
 * Synchronize DELAY_MS with DELAY_MS from elapsed_time.sjs
 */
const DELAY_MS = 200;
const SLOW_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "elapsed_time.sjs";

add_task(async function testLongElapsedTime() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(tabBrowser) {
      const flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK;
      let listener;

      let stateChangeWaiter = new Promise(resolve => {
        listener = {
          onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
            if (!aWebProgress.isTopLevel) {
              return;
            }
            const isTopLevel = aWebProgress.isTopLevel;
            const isStop = aStateFlags & Ci.nsIWebProgressListener.STATE_STOP;
            const isNetwork =
              aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
            const isWindow =
              aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW;
            const isLoadingDocument = aWebProgress.isLoadingDocument;

            if (
              isTopLevel &&
              isStop &&
              isWindow &&
              isNetwork &&
              !isLoadingDocument
            ) {
              aRequest.QueryInterface(Ci.nsIRemoteWebProgressRequest);
              if (aRequest.elapsedLoadTimeMS >= DELAY_MS) {
                resolve(true);
              }
            }
          },
        };
      });
      tabBrowser.addProgressListener(listener, flags);

      BrowserTestUtils.loadURI(tabBrowser, SLOW_PAGE);
      await BrowserTestUtils.browserLoaded(tabBrowser);
      let pass = await stateChangeWaiter;

      tabBrowser.removeProgressListener(listener);

      ok(
        pass,
        "Bug 1559657: Check that the elapsedLoadTimeMS in RemoteWebProgress meets expectations."
      );
    }
  );
});
