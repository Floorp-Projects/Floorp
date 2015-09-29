/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Raymond Lee <raymond@appcoast.com>
 */

"use strict";

const TEST_URL = 'data:text/html,<script>window.onbeforeunload=' +
                 'function(e){e.returnValue="?"}</script>';

var contentWindow;
var activeGroup;

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

Components.utils.import("resource://gre/modules/Promise.jsm", this);

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(win => {
    contentWindow = win.TabView.getContentWindow();
    activeGroup = contentWindow.GroupItems.getActiveGroupItem();

    win.gBrowser.browsers[0].loadURI("data:text/html,<p>test for bug 626455, tab1");

    let tab = win.gBrowser.addTab(TEST_URL);
    afterAllTabsLoaded(() => testStayOnPage(win, tab));
  });
}

function testStayOnPage(win, blockingTab) {
  let browser = blockingTab.linkedBrowser;
  waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
    // stay on page
    btnStay.click();

    executeSoon(function () {
      showTabView(function () {
        is(win.gBrowser.tabs.length, 1,
           "The total number of tab is 1 when staying on the page");

        // The other initial tab has been closed when trying to close the tab
        // group. The only tab left is the one with the onbeforeunload dialog.
        let url = win.gBrowser.browsers[0].currentURI.spec;
        ok(url.includes("onbeforeunload"), "The open tab is the expected one");

        is(contentWindow.GroupItems.getActiveGroupItem(), activeGroup,
           "Active group is still the same");

        is(contentWindow.GroupItems.groupItems.length, 1,
           "Only one group is open");

        // start the next test
        testLeavePage(win, win.gBrowser.tabs[0]);
      }, win);
    });
  });

  closeGroupItem(activeGroup);
}

function testLeavePage(win, blockingTab) {
  let browser = blockingTab.linkedBrowser;
  waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
    // Leave page
    btnLeave.click();
  });

  whenGroupClosed(activeGroup, () => finishTest(win));
  closeGroupItem(activeGroup);
}

function finishTest(win) {
  is(win.gBrowser.tabs.length, 1,
     "The total number of tab is 1 after leaving the page");
  is(contentWindow.TabItems.getItems().length, 1,
     "The total number of tab items is 1 after leaving the page");

  let location = win.gBrowser.browsers[0].currentURI.spec;
  is(location, BROWSER_NEW_TAB_URL, "The open tab is the expected one");

  isnot(contentWindow.GroupItems.getActiveGroupItem(), activeGroup,
     "Active group is no longer the same");

  is(contentWindow.GroupItems.groupItems.length, 1,
     "Only one group is open");

  contentWindow = null;
  activeGroup = null;
  promiseWindowClosed(win).then(finish);
}

// ----------
function whenGroupClosed(group, callback) {
  group.addSubscriber("close", function onClose() {
    group.removeSubscriber("close", onClose);
    callback();
  });
}
