/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for Bug 670318
 *
 * When LoadEntry() is called on a browser that has multiple duplicate history
 * entries, history.index can end up out of range (>= history.count).
 */

const URL =
  "http://mochi.test:8888/browser/docshell/test/browser/file_bug670318.html";

add_task(async function test() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    async function(browser) {
    let history = browser.browsingContext.sessionHistory;
    let count = 0;

    let testDone = {};
    testDone.promise = new Promise(resolve => { testDone.resolve = resolve; });

    let listener = {
      async OnHistoryNewEntry(aNewURI) {
        if (aNewURI.spec == URL && 5 == ++count) {
          history.removeSHistoryListener(listener);
          await ContentTask.spawn(browser, null, async () => {
            let promise = new Promise(resolve => {
              addEventListener("load", evt => {
                let history = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
                Assert.ok(history.index < history.count, "history.index is valid");
                resolve();
              }, { capture: true, once: true });
            });

            content.location.reload();
            await promise;
          });
          testDone.resolve();
        }
      },

      OnHistoryReload: () => true,
      OnHistoryGotoIndex: () => {},
      OnHistoryPurge: () => {},
      OnHistoryReplaceEntry: () => {
        // The initial load of about:blank causes a transient entry to be
        // created, so our first navigation to a real page is a replace
        // instead of a new entry.
        ++count;
      },

      QueryInterface: ChromeUtils.generateQI([Ci.nsISHistoryListener,
                                              Ci.nsISupportsWeakReference]),
    };

    history.addSHistoryListener(listener);
    BrowserTestUtils.loadURI(browser, URL);

    await testDone.promise;
  });
});
