/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that the debugger attaches to the right tab when multiple windows
// are open.

var gTab1 = null;
var gTab1Actor = null;

var gSecondWindow = null;

var gClient = null;
var windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Ci.nsIWindowMediator);

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    test_first_tab();
  });
}

function test_first_tab()
{
  gTab1 = addTab(TAB1_URL, function() {
    gClient.listTabs(function(aResponse) {
      for each (let tab in aResponse.tabs) {
        if (tab.url == TAB1_URL) {
          gTab1Actor = tab.actor;
        }
      }
      ok(gTab1Actor, "Should find a tab actor for tab1.");
      is(aResponse.selected, 1, "Tab1 is selected.");
      test_open_window();
    });
  });
}

function test_open_window()
{
  gSecondWindow = window.open(TAB2_URL, "secondWindow");
  ok(!!gSecondWindow, "Second window created.");
  gSecondWindow.focus();
  let top = windowMediator.getMostRecentWindow("navigator:browser");
  var main2 = gSecondWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIWebNavigation)
                   .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindow);
  is(top, main2, "The second window is on top.");
  executeSoon(function() {
    gClient.listTabs(function(aResponse) {
      is(aResponse.selected, 2, "Tab2 is selected.");

      test_focus_first();
    });
  });
}

function test_focus_first()
{
  window.content.addEventListener("focus", function onFocus() {
    window.content.removeEventListener("focus", onFocus, false);
    let top = windowMediator.getMostRecentWindow("navigator:browser");
    var main1 = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                     .getInterface(Components.interfaces.nsIWebNavigation)
                     .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                     .rootTreeItem
                     .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                     .getInterface(Components.interfaces.nsIDOMWindow);
    is(top, main1, "The first window is on top.");

    gClient.listTabs(function(aResponse) {
      is(aResponse.selected, 1, "Tab1 is selected after focusing on it.");

      test_remove_tab();
    });
  }, false);
  window.content.focus();
}

function test_remove_tab()
{
  gSecondWindow.close();
  gSecondWindow = null;
  removeTab(gTab1);
  gTab1 = null;
  gClient.listTabs(function(aResponse) {
    // Verify that tabs are no longer included in listTabs.
    let foundTab1 = false;
    let foundTab2 = false;
    for (let tab of aResponse.tabs) {
      if (tab.url == TAB1_URL) {
        foundTab1 = true;
      } else if (tab.url == TAB2_URL) {
        foundTab2 = true;
      }
    }
    ok(!foundTab1, "Tab1 should be gone.");
    ok(!foundTab2, "Tab2 should be gone.");
    is(aResponse.selected, 0, "The original tab is selected.");
    finish_test();
  });
}

function finish_test()
{
  gClient.close(function() {
    finish();
  });
}
