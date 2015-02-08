/*
 * This test checks that window activation state is set properly with multiple tabs.
 */

let testPage = "data:text/html,<body><style>:-moz-window-inactive { background-color: red; }</style><div id='area'></div></body>";

let colorChangeNotifications = 0;
let otherWindow;

let browser1, browser2;

function test() {
  waitForExplicitFinish();
  waitForFocus(reallyRunTests);
}

function reallyRunTests() {

  let tab1 = gBrowser.addTab();
  let tab2 = gBrowser.addTab();
  browser1 = gBrowser.getBrowserForTab(tab1);
  browser2 = gBrowser.getBrowserForTab(tab2);

  gURLBar.focus();

  var loadCount = 0;
  function check()
  {
    // wait for both tabs to load
    if (++loadCount != 2) {
      return;
    }

    browser1.removeEventListener("load", check, true);
    browser2.removeEventListener("load", check, true);

    sendGetBackgroundRequest(true);
  }

  // The test performs four checks, using -moz-window-inactive on two child tabs.
  // First, the initial state should be transparent. The second check is done
  // while another window is focused. The third check is done after that window
  // is closed and the main window focused again. The fourth check is done after
  // switching to the second tab.
  window.messageManager.addMessageListener("Test:BackgroundColorChanged", function(message) {
    colorChangeNotifications++;

    switch (colorChangeNotifications) {
      case 1:
        is(message.data.color, "transparent", "first window initial");
        break;
      case 2:
        is(message.data.color, "transparent", "second window initial");
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
        is(message.data.color, "transparent", "first window raised");
        break;
      case 6:
        is(message.data.color, "transparent", "second window raised");
        gBrowser.selectedTab = tab2;
        break;
      case 7:
        is(message.data.color, "transparent", "first window after tab switch");
        break;
      case 8:
        is(message.data.color, "transparent", "second window after tab switch");
        finishTest();
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

  browser1.addEventListener("load", check, true);
  browser2.addEventListener("load", check, true);
  browser1.contentWindow.location = testPage;
  browser2.contentWindow.location = testPage;

  browser1.messageManager.loadFrameScript("data:,(" + childFunction.toString() + ")();", true);
  browser2.messageManager.loadFrameScript("data:,(" + childFunction.toString() + ")();", true);

  gBrowser.selectedTab = tab1;
}

function sendGetBackgroundRequest(ifChanged)
{
  browser1.messageManager.sendAsyncMessage("Test:GetBackgroundColor", { ifChanged: ifChanged });
  browser2.messageManager.sendAsyncMessage("Test:GetBackgroundColor", { ifChanged: ifChanged });
}

function runOtherWindowTests() {
  otherWindow = window.open("data:text/html,<body>Hi</body>", "", "chrome");
  waitForFocus(function () {
    sendGetBackgroundRequest(true);
  }, otherWindow);
}

function finishTest()
{
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
  otherWindow = null;
  finish();
}

function childFunction()
{
  let oldColor = null;

  let expectingResponse = false;
  let ifChanged = true;

  addMessageListener("Test:GetBackgroundColor", function(message) {
    expectingResponse = true;
    ifChanged = message.data.ifChanged;
  });

  content.addEventListener("focus", function () {
    sendAsyncMessage("Test:FocusReceived", { });
  }, false);

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
      windowGotActivate = true;;
    });
  
  content.addEventListener("deactivate", function() {
      windowGotDeactivate = true;
    });

  content.setInterval(function () {
    if (!expectingResponse) {
      return;
    }

    let area = content.document.getElementById("area");
    if (!area) {
      return; /* hasn't loaded yet */
    }

    let color = content.getComputedStyle(area, "").backgroundColor;
    if (oldColor != color || !ifChanged) {
      expectingResponse = false;
      oldColor = color;
      sendAsyncMessage("Test:BackgroundColorChanged", { color: color });
    }
  }, 20);
}
