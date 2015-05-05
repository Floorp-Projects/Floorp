/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let assertNumberOfTabs = function (num, msg) {
    is(gBrowser.tabs.length, num, msg);
  }

  let assertNumberOfVisibleTabs = function (num, msg) {
    is(gBrowser.visibleTabs.length, num, msg);
  }

  let assertNumberOfPinnedTabs = function (num, msg) {
    is(gBrowser._numPinnedTabs, num, msg);
  }

  waitForExplicitFinish();

  // check prerequisites
  assertNumberOfTabs(1, "we start off with one tab");

  // setup
  let tab = gBrowser.addTab("about:mozilla");

  whenTabIsLoaded(tab, function () {
    // hide the newly created tab
    assertNumberOfVisibleTabs(2, "there are two visible tabs");
    gBrowser.showOnlyTheseTabs([gBrowser.tabs[0]]);
    assertNumberOfVisibleTabs(1, "there is one visible tab");
    ok(tab.hidden, "newly created tab is now hidden");

    // close and restore hidden tab
    promiseRemoveTab(tab).then(() => {
      tab = ss.undoCloseTab(window, 0);

      // check that everything was restored correctly, clean up and finish
      whenTabIsLoaded(tab, function () {
        is(tab.linkedBrowser.currentURI.spec, "about:mozilla", "restored tab has correct url");

        gBrowser.removeTab(tab);
        finish();
      });
    });
  });
}

function whenTabIsLoaded(tab, callback) {
  tab.linkedBrowser.addEventListener("load", function onLoad() {
    tab.linkedBrowser.removeEventListener("load", onLoad, true);
    callback();
  }, true);
}
