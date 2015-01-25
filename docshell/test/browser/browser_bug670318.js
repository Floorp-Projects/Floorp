/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for Bug 670318
 *
 * When LoadEntry() is called on a browser that has multiple duplicate history
 * entries, history.index can end up out of range (>= history.count).
 */

const URL = "http://mochi.test:8888/browser/docshell/test/browser/file_bug670318.html";

function test() {
  waitForExplicitFinish();

  let count = 0, historyListenerRemoved = false;

  let listener = {
    OnHistoryNewEntry: function (aNewURI) {
      if (aNewURI.spec == URL && 5 == ++count) {
        browser.addEventListener("load", function onLoad() {
          browser.removeEventListener("load", onLoad, true);

          ok(history.index < history.count, "history.index is valid");
          finish();
        }, true);

        history.removeSHistoryListener(listener);
        historyListenerRemoved = true;

        executeSoon(function () { BrowserReload(); });
      }

      return true;
    },

    OnHistoryReload: () => true,
    OnHistoryGoBack: () => true,
    OnHistoryGoForward: () => true,
    OnHistoryGotoIndex: () => true,
    OnHistoryPurge: () => true,
    OnHistoryReplaceEntry: () => true,

    QueryInterface: XPCOMUtils.generateQI([Ci.nsISHistoryListener,
                                           Ci.nsISupportsWeakReference])
  };

  let tab = gBrowser.loadOneTab(URL, {inBackground: false});
  let browser = tab.linkedBrowser;
  let history = browser.sessionHistory;

  history.addSHistoryListener(listener);

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);

    if (!historyListenerRemoved)
      history.removeSHistoryListener(listener);
  });
}
