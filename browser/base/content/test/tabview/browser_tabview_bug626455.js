/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Raymond Lee <raymond@appcoast.com>
 */

const TEST_URL = 'data:text/html,<script>window.onbeforeunload=' +
                 'function(e){e.returnValue="?"}</script><body ' +
                 'onunload="alert(\'onunload\')" onpagehide="' +
                 'alert(\'onpagehide\')"></body>';

let contentWindow;
let activeGroup;

function test() {
  waitForExplicitFinish();

  showTabView(function () {
    contentWindow = TabView.getContentWindow();
    activeGroup = contentWindow.GroupItems.getActiveGroupItem();

    gBrowser.browsers[0].contentWindow.location =
      "data:text/html,<p>test for bug 626455, tab1";
    gBrowser.addTab(TEST_URL);

    afterAllTabsLoaded(testStayOnPage);
  });
}

function testStayOnPage() {
  whenDialogOpened(function (dialog) {
    // stay on page
    dialog.cancelDialog();

    executeSoon(function () {
      showTabView(function () {
        is(gBrowser.tabs.length, 1,
           "The total number of tab is 1 when staying on the page");

        let location = gBrowser.browsers[0].contentWindow.location.toString();
        isnot(location.indexOf("onbeforeunload"), -1,
              "The open tab is the expected one");

        is(contentWindow.GroupItems.getActiveGroupItem(), activeGroup,
           "Active group is still the same");

        is(contentWindow.GroupItems.groupItems.length, 1,
           "Only one group is open");

        // start the next test
        testLeavePage();
      });
    });
  });

  closeGroupItem(activeGroup);
}

function testLeavePage() {
  let dialogsAccepted = 0;

  whenDialogOpened(function onDialogOpened(dialog) {
    if (++dialogsAccepted < 3)
      whenDialogOpened(onDialogOpened);

    // Leave page
    dialog.acceptDialog();
  });

  whenGroupClosed(activeGroup, finishTest);
  closeGroupItem(activeGroup);
}

function finishTest() {
  is(gBrowser.tabs.length, 1,
     "The total number of tab is 1 after leaving the page");
  is(contentWindow.TabItems.getItems().length, 1,
     "The total number of tab items is 1 after leaving the page");

  let location = gBrowser.browsers[0].contentWindow.location.toString();
  is(location, "about:blank", "The open tab is the expected one");

  isnot(contentWindow.GroupItems.getActiveGroupItem(), activeGroup,
     "Active group is no longer the same");

  is(contentWindow.GroupItems.groupItems.length, 1,
     "Only one group is open");

  finish();
}

// ----------
function whenGroupClosed(group, callback) {
  group.addSubscriber("close", function onClose() {
    group.removeSubscriber("close", onClose);
    callback();
  });
}

// ----------
function whenDialogOpened(callback) {
  let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
           .getService(Ci.nsIWindowMediator);

  let listener = {
    onCloseWindow: function () {},
    onWindowTitleChange: function () {},

    onOpenWindow: function (xulWin) {
      let domWin = xulWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);

      whenWindowLoaded(domWin, function () {
        let dialog = domWin.document.querySelector("dialog");
        if (dialog) {
          wm.removeListener(listener);
          callback(dialog);
        }
      });
    }
  };

  wm.addListener(listener);
}
