/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = 'data:text/html,<script>window.onbeforeunload=' +
                 'function(e){e.returnValue="?"}</script>';

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
  whenDialogOpened(function (dialog) {
    groupItemTwo.addSubscriber("groupShown", function onShown() {
      groupItemTwo.removeSubscriber("groupShown", onShown);

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
    dialog.cancelDialog();
  });

  closeGroupItem(groupItemTwo);
}

function testLeavePage(contentWindow, groupItemOne, groupItemTwo) {
  whenDialogOpened(function (dialog) {
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
    dialog.acceptDialog();
  });

  closeGroupItem(groupItemTwo);
}

// ----------
function whenDialogOpened(callback) {
  let listener = {
    onCloseWindow: function () {},
    onWindowTitleChange: function () {},

    onOpenWindow: function (xulWin) {
      let domWin = xulWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);

      whenWindowLoaded(domWin, function () {
        let dialog = domWin.document.querySelector("dialog");
        if (dialog) {
          Services.wm.removeListener(listener);
          callback(dialog);
        }
      });
    }
  };

  Services.wm.addListener(listener);
}
