const windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

function test()
{
  waitForExplicitFinish();

  // Need to enable modal dialogs for this test.
  Services.prefs.setBoolPref("prompts.tab_modal.enabled", false);

  windowMediator.addListener(promptListener);

  // Open a new tab and have that tab open a new window. This is done to
  // ensure that the new window is a normal browser window.
  var script = "window.open('data:text/html,<button id=\"button\" onclick=\"window.close(); alert(5);\">Close</button>', null, 'width=200,height=200');";
  gBrowser.selectedTab = 
    gBrowser.addTab("data:text/html,<body onload='setTimeout(dotest, 0)'><script>function dotest() { " + script + "}</script></body>");
}

function windowOpened(win)
{
  // Wait for the page in the window to load.
  waitForFocus(clickButton, win.content);
}

function clickButton(win)
{
  // Set the window in the prompt listener to indicate that the alert window
  // is now expected to open.
  promptListener.window = win;

  // Click the Close button in the window.
  EventUtils.synthesizeMouseAtCenter(win.content.document.getElementById("button"), { }, win);

  windowMediator.removeListener(promptListener);
  gBrowser.removeTab(gBrowser.selectedTab);

  is(promptListener.message, "window appeared", "modal prompt closer didn't crash");
  Services.prefs.clearUserPref("prompts.tab_modal.enabled", false);
  finish();
}

var promptListener = {
  onWindowTitleChange: function () {},
  onOpenWindow: function (win) {
    let domWin = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    if (!promptListener.window) {
      // The first window that is open is the one opened by the new tab.
      waitForFocus(windowOpened, domWin);
    }
    else {
      // The second window is the alert opened when clicking the Close button in the window
      ok(promptListener.window.closed, "window has closed");

      // Assign a message so that it can be checked just before the test
      // finishes to ensure that the alert opened properly.
      promptListener.message = "window appeared";
      executeSoon(function () { domWin.close() });
    }
  },
  onCloseWindow: function () {}
};
