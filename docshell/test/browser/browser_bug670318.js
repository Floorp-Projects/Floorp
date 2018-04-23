/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for Bug 670318
 *
 * When LoadEntry() is called on a browser that has multiple duplicate history
 * entries, history.index can end up out of range (>= history.count).
 */

const URL = "http://mochi.test:8888/browser/docshell/test/browser/file_bug670318.html";

add_task(async function test() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    async function(browser) {
    await ContentTask.spawn(browser, URL, async function(URL) {
      let history = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
      let count = 0;

      let testDone = {};
      testDone.promise = new Promise(resolve => { testDone.resolve = resolve; });

      let listener = {
        OnHistoryNewEntry: function (aNewURI) {
          if (aNewURI.spec == URL && 5 == ++count) {
            addEventListener("load", function onLoad() {
              removeEventListener("load", onLoad, true);

              Assert.ok(history.index < history.count, "history.index is valid");
              testDone.resolve();
            }, true);

            history.legacySHistory.removeSHistoryListener(listener);
            delete content._testListener;
            content.setTimeout(() => { content.location.reload(); }, 0);
          }

          return true;
        },

        OnHistoryReload: () => true,
        OnHistoryGoBack: () => true,
        OnHistoryGoForward: () => true,
        OnHistoryGotoIndex: () => true,
        OnHistoryPurge: () => true,
        OnHistoryReplaceEntry: () => {
          // The initial load of about:blank causes a transient entry to be
          // created, so our first navigation to a real page is a replace
          // instead of a new entry.
          ++count;
          return true;
        },

        QueryInterface: ChromeUtils.generateQI([Ci.nsISHistoryListener,
                                                Ci.nsISupportsWeakReference])
      };

      history.legacySHistory.addSHistoryListener(listener);
      // Since listener implements nsISupportsWeakReference, we are
      // responsible for keeping it alive so that the GC doesn't clear
      // it before the test completes. We do this by anchoring the listener
      // to the content global window, and clearing it just before the test
      // completes.
      content._testListener = listener;
      content.location = URL;

      await testDone.promise;
    });
  });
});
