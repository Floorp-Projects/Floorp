/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for Bug 1594938
 *
 * If a session history listener blocks reloads we shouldn't crash.
 */

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "https://example.com/" },
    async function (browser) {
      if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
        await SpecialPowers.spawn(browser, [], async function () {
          let history = this.content.docShell.QueryInterface(
            Ci.nsIWebNavigation
          ).sessionHistory;

          let testDone = {};
          testDone.promise = new Promise(resolve => {
            testDone.resolve = resolve;
          });

          let listenerCalled = false;
          let listener = {
            OnHistoryNewEntry: () => {},
            OnHistoryReload: () => {
              listenerCalled = true;
              this.content.setTimeout(() => {
                testDone.resolve();
              });
              return false;
            },
            OnHistoryGotoIndex: () => {},
            OnHistoryPurge: () => {},
            OnHistoryReplaceEntry: () => {},

            QueryInterface: ChromeUtils.generateQI([
              "nsISHistoryListener",
              "nsISupportsWeakReference",
            ]),
          };

          history.legacySHistory.addSHistoryListener(listener);

          history.reload(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
          await testDone.promise;

          Assert.ok(listenerCalled, "reloads were blocked");

          history.legacySHistory.removeSHistoryListener(listener);
        });

        return;
      }

      let history = browser.browsingContext.sessionHistory;

      let testDone = {};
      testDone.promise = new Promise(resolve => {
        testDone.resolve = resolve;
      });

      let listenerCalled = false;
      let listener = {
        OnHistoryNewEntry: () => {},
        OnHistoryReload: () => {
          listenerCalled = true;
          setTimeout(() => {
            testDone.resolve();
          });
          return false;
        },
        OnHistoryGotoIndex: () => {},
        OnHistoryPurge: () => {},
        OnHistoryReplaceEntry: () => {},

        QueryInterface: ChromeUtils.generateQI([
          "nsISHistoryListener",
          "nsISupportsWeakReference",
        ]),
      };

      history.addSHistoryListener(listener);

      await SpecialPowers.spawn(browser, [], () => {
        let history = this.content.docShell.QueryInterface(
          Ci.nsIWebNavigation
        ).sessionHistory;
        history.reload(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
      });
      await testDone.promise;

      Assert.ok(listenerCalled, "reloads were blocked");

      history.removeSHistoryListener(listener);
    }
  );
});
