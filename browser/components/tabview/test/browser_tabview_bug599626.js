/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = 'data:text/html,<script>window.onbeforeunload=' +
                 'function(e){e.returnValue="?"}</script>';

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

function test() {
  waitForExplicitFinish();
  showTabView(onTabViewShown);
}

function onTabViewShown() {
  let contentWindow = TabView.getContentWindow();
  let groupItemOne = contentWindow.GroupItems.getActiveGroupItem();
  let groupItemTwo = createGroupItemWithTabs(window, 300, 300, 10, [TEST_URL]);

  afterAllTabsLoaded(function () {
    testStayOnPage(contentWindow, groupItemOne, groupItemTwo);
  });
}

function testStayOnPage(contentWindow, groupItemOne, groupItemTwo) {
  // We created a new tab group with a second tab above, so let's
  // pick that second tab here and wait for its onbeforeunload dialog.
  let browser = gBrowser.browsers[1];
  waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
    executeSoon(function () {
      is(gBrowser.tabs.length, 2, 
         "The total number of tab is 2 when staying on the page");
      is(contentWindow.TabItems.getItems().length, 2, 
         "The total number of tab items is 2 when staying on the page");

      showTabView(function () {
        // start the next test
        testLeavePage(contentWindow, groupItemOne, groupItemTwo);
      });
    });

    // stay on page
    btnStay.click();
  });

  closeGroupItem(groupItemTwo);
}

function testLeavePage(contentWindow, groupItemOne, groupItemTwo) {
  // The second tab hasn't been closed yet because we chose to stay. Wait
  // for the onbeforeunload dialog again and leave the page this time.
  let browser = gBrowser.browsers[1];
  waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
    // clean up and finish the test
    groupItemTwo.addSubscriber("close", function onClose() {
      groupItemTwo.removeSubscriber("close", onClose);

      is(gBrowser.tabs.length, 1,
         "The total number of tab is 1 after leaving the page");
      is(contentWindow.TabItems.getItems().length, 1, 
         "The total number of tab items is 1 after leaving the page");

      hideTabView(finish);
    });

    // Leave page
    btnLeave.click();
  });

  closeGroupItem(groupItemTwo);
}
