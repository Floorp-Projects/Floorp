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
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
        await ContentTask.spawn(browser, URL, async function (URL) {
          let history = docShell.QueryInterface(
            Ci.nsIWebNavigation
          ).sessionHistory;
          let count = 0;

          let testDone = {};
          testDone.promise = new Promise(resolve => {
            testDone.resolve = resolve;
          });

          // Since listener implements nsISupportsWeakReference, we are
          // responsible for keeping it alive so that the GC doesn't clear
          // it before the test completes. We do this by anchoring the listener
          // to the message manager, and clearing it just before the test
          // completes.
          this._testListener = {
            owner: this,
            OnHistoryNewEntry(aNewURI) {
              info("OnHistoryNewEntry " + aNewURI.spec + ", " + count);
              if (aNewURI.spec == URL && 5 == ++count) {
                addEventListener(
                  "load",
                  function onLoad() {
                    Assert.ok(
                      history.index < history.count,
                      "history.index is valid"
                    );
                    testDone.resolve();
                  },
                  { capture: true, once: true }
                );

                history.legacySHistory.removeSHistoryListener(
                  this.owner._testListener
                );
                delete this.owner._testListener;
                this.owner = null;
                content.setTimeout(() => {
                  content.location.reload();
                }, 0);
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

            QueryInterface: ChromeUtils.generateQI([
              "nsISHistoryListener",
              "nsISupportsWeakReference",
            ]),
          };

          history.legacySHistory.addSHistoryListener(this._testListener);
          content.location = URL;

          await testDone.promise;
        });

        return;
      }

      let history = browser.browsingContext.sessionHistory;
      let count = 0;

      let testDone = {};
      testDone.promise = new Promise(resolve => {
        testDone.resolve = resolve;
      });

      let listener = {
        async OnHistoryNewEntry(aNewURI) {
          if (aNewURI.spec == URL && 5 == ++count) {
            history.removeSHistoryListener(listener);
            await ContentTask.spawn(browser, null, () => {
              return new Promise(resolve => {
                addEventListener(
                  "load",
                  evt => {
                    let history = docShell.QueryInterface(
                      Ci.nsIWebNavigation
                    ).sessionHistory;
                    Assert.ok(
                      history.index < history.count,
                      "history.index is valid"
                    );
                    resolve();
                  },
                  { capture: true, once: true }
                );

                content.location.reload();
              });
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

        QueryInterface: ChromeUtils.generateQI([
          "nsISHistoryListener",
          "nsISupportsWeakReference",
        ]),
      };

      history.addSHistoryListener(listener);
      BrowserTestUtils.startLoadingURIString(browser, URL);

      await testDone.promise;
    }
  );
});
