/*
 * This test checks that window activation state is set properly with multiple tabs.
 */

/* eslint-env mozilla/frame-script */

var testPage = "data:text/html;charset=utf-8,<body><style>:-moz-window-inactive { background-color: red; }</style><div id='area'></div></body>";

var colorChangeNotifications = 0;
var otherWindow;

var browser1, browser2;

add_task(async function reallyRunTests() {

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  browser1 = tab1.linkedBrowser;

  // This can't use openNewForegroundTab because if we focus tab2 now, we
  // won't send a focus event during test 6, further down in this file.
  let tab2 = BrowserTestUtils.addTab(gBrowser, testPage);
  browser2 = tab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser2);

  browser1.messageManager.loadFrameScript("data:,(" + childFunction.toString() + ")();", true);
  browser2.messageManager.loadFrameScript("data:,(" + childFunction.toString() + ")();", true);

  gURLBar.focus();

  let testFinished = {};
  testFinished.promise = new Promise(resolve => testFinished.resolve = resolve);

  // The test performs four checks, using -moz-window-inactive on two child tabs.
  // First, the initial state should be transparent. The second check is done
  // while another window is focused. The third check is done after that window
  // is closed and the main window focused again. The fourth check is done after
  // switching to the second tab.
  window.messageManager.addMessageListener("Test:BackgroundColorChanged", function(message) {
    colorChangeNotifications++;

    switch (colorChangeNotifications) {
      case 1:
        is(message.data.color, "rgba(0, 0, 0, 0)", "first window initial");
        break;
      case 2:
        is(message.data.color, "rgba(0, 0, 0, 0)", "second window initial");
        runOtherWindowTests();
        break;
      case 3:
        is(message.data.color, "rgb(255, 0, 0)", "first window lowered");
        break;
      case 4:
        is(message.data.color, "rgb(255, 0, 0)", "second window lowered");
        sendGetBackgroundRequest(true);
        otherWindow.close();
        break;
      case 5:
        is(message.data.color, "rgba(0, 0, 0, 0)", "first window raised");
        break;
      case 6:
        is(message.data.color, "rgba(0, 0, 0, 0)", "second window raised");
        gBrowser.selectedTab = tab2;
        break;
      case 7:
        is(message.data.color, "rgba(0, 0, 0, 0)", "first window after tab switch");
        break;
      case 8:
        is(message.data.color, "rgba(0, 0, 0, 0)", "second window after tab switch");
        testFinished.resolve();
        break;
      case 9:
        ok(false, "too many color change notifications");
        break;
    }
  });

  window.messageManager.addMessageListener("Test:FocusReceived", function(message) {
    // No color change should occur after a tab switch.
    if (colorChangeNotifications == 6) {
      sendGetBackgroundRequest(false);
    }
  });

  window.messageManager.addMessageListener("Test:ActivateEvent", function(message) {
    ok(message.data.ok, "Test:ActivateEvent");
  });

  window.messageManager.addMessageListener("Test:DeactivateEvent", function(message) {
    ok(message.data.ok, "Test:DeactivateEvent");
  });

  gBrowser.selectedTab = tab1;

  // Start the test.
  sendGetBackgroundRequest(true);

  await testFinished.promise;

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  otherWindow = null;
});

function sendGetBackgroundRequest(ifChanged) {
  browser1.messageManager.sendAsyncMessage("Test:GetBackgroundColor", { ifChanged });
  browser2.messageManager.sendAsyncMessage("Test:GetBackgroundColor", { ifChanged });
}

function runOtherWindowTests() {
  otherWindow = window.open("data:text/html;charset=utf-8,<body>Hi</body>", "", "chrome");
  waitForFocus(function() {
    sendGetBackgroundRequest(true);
  }, otherWindow);
}

function childFunction() {
  let oldColor = null;

  let expectingResponse = false;
  let ifChanged = true;

  addMessageListener("Test:GetBackgroundColor", function(message) {
    expectingResponse = true;
    ifChanged = message.data.ifChanged;
  });

  content.addEventListener("focus", function() {
    sendAsyncMessage("Test:FocusReceived", { });
  });

  var windowGotActivate = false;
  var windowGotDeactivate = false;
  addEventListener("activate", function() {
      sendAsyncMessage("Test:ActivateEvent", { ok: !windowGotActivate });
      windowGotActivate = false;
    });

  addEventListener("deactivate", function() {
      sendAsyncMessage("Test:DeactivateEvent", { ok: !windowGotDeactivate });
      windowGotDeactivate = false;
    });
  content.addEventListener("activate", function() {
      windowGotActivate = true;
    });

  content.addEventListener("deactivate", function() {
      windowGotDeactivate = true;
    });

  content.setInterval(function() {
    if (!expectingResponse) {
      return;
    }

    let area = content.document.getElementById("area");
    if (!area) {
      return; /* hasn't loaded yet */
    }

    let color = content.getComputedStyle(area).backgroundColor;
    if (oldColor != color || !ifChanged) {
      expectingResponse = false;
      oldColor = color;
      sendAsyncMessage("Test:BackgroundColorChanged", { color });
    }
  }, 20);
}
