/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests for bug 537013 to ensure proper tab-sequestration of find bar. */

var tabs = [];
var texts = [
  "This side up.",
  "The world is coming to an end. Please log off.",
  "Klein bottle for sale. Inquire within.",
  "To err is human; to forgive is not company policy."
];

var Clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
var HasFindClipboard = Clipboard.supportsFindClipboard();

function addTabWithText(aText, aCallback) {
  let newTab = gBrowser.addTab("data:text/html;charset=utf-8,<h1 id='h1'>" +
                               aText + "</h1>");
  tabs.push(newTab);
  gBrowser.selectedTab = newTab;
}

function setFindString(aString) {
  gFindBar.open();
  gFindBar._findField.focus();
  gFindBar._findField.select();
  EventUtils.sendString(aString);
  is(gFindBar._findField.value, aString, "Set the field correctly!");
}

var newWindow;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    while (tabs.length) {
      gBrowser.removeTab(tabs.pop());
    }
  });
  texts.forEach(aText => addTabWithText(aText));

  // Set up the first tab
  gBrowser.selectedTab = tabs[0];

  setFindString(texts[0]);
  // Turn on highlight for testing bug 891638
  gFindBar.toggleHighlight(true);

  // Make sure the second tab is correct, then set it up
  gBrowser.selectedTab = tabs[1];
  gBrowser.selectedTab.addEventListener("TabFindInitialized", continueTests1);
  // Initialize the findbar
  gFindBar;
}
function continueTests1() {
  gBrowser.selectedTab.removeEventListener("TabFindInitialized",
                                           continueTests1);
  ok(true, "'TabFindInitialized' event properly dispatched!");
  ok(gFindBar.hidden, "Second tab doesn't show find bar!");
  gFindBar.open();
  is(gFindBar._findField.value, texts[0],
     "Second tab kept old find value for new initialization!");
  setFindString(texts[1]);

  // Confirm the first tab is still correct, ensure re-hiding works as expected
  gBrowser.selectedTab = tabs[0];
  ok(!gFindBar.hidden, "First tab shows find bar!");
  // When the Find Clipboard is supported, this test not relevant.
  if (!HasFindClipboard)
    is(gFindBar._findField.value, texts[0], "First tab persists find value!");
  ok(gFindBar.getElement("highlight").checked,
     "Highlight button state persists!");

  // While we're here, let's test the backout of bug 253793.
  gBrowser.reload();
  gBrowser.addEventListener("DOMContentLoaded", continueTests2, true);
}

function continueTests2() {
  gBrowser.removeEventListener("DOMContentLoaded", continueTests2, true);
  ok(gFindBar.getElement("highlight").checked, "Highlight never reset!");
  continueTests3();
}

function continueTests3() {
  ok(gFindBar.getElement("highlight").checked, "Highlight button reset!");
  gFindBar.close();
  ok(gFindBar.hidden, "First tab doesn't show find bar!");
  gBrowser.selectedTab = tabs[1];
  ok(!gFindBar.hidden, "Second tab shows find bar!");
  // Test for bug 892384
  is(gFindBar._findField.getAttribute("focused"), "true",
     "Open findbar refocused on tab change!");
  gURLBar.focus();
  gBrowser.selectedTab = tabs[0];
  ok(gFindBar.hidden, "First tab doesn't show find bar!");

  // Set up a third tab, no tests here
  gBrowser.selectedTab = tabs[2];
  setFindString(texts[2]);

  // Now we jump to the second, then first, and then fourth
  gBrowser.selectedTab = tabs[1];
  // Test for bug 892384
  ok(!gFindBar._findField.hasAttribute("focused"),
     "Open findbar not refocused on tab change!");
  gBrowser.selectedTab = tabs[0];
  gBrowser.selectedTab = tabs[3];
  ok(gFindBar.hidden, "Fourth tab doesn't show find bar!");
  is(gFindBar, gBrowser.getFindBar(), "Find bar is right one!");
  gFindBar.open();
  // Disabled the following assertion due to intermittent failure on OSX 10.6 Debug.
  if (!HasFindClipboard) {
    is(gFindBar._findField.value, texts[1],
       "Fourth tab has second tab's find value!");
  }

  newWindow = gBrowser.replaceTabWithWindow(tabs.pop());
  whenDelayedStartupFinished(newWindow, checkNewWindow);
}

// Test that findbar gets restored when a tab is moved to a new window.
function checkNewWindow() {
  ok(!newWindow.gFindBar.hidden, "New window shows find bar!");
  // Disabled the following assertion due to intermittent failure on OSX 10.6 Debug.
  if (!HasFindClipboard) {
    is(newWindow.gFindBar._findField.value, texts[1],
       "New window find bar has correct find value!");
    ok(!newWindow.gFindBar.getElement("find-next").disabled,
       "New window findbar has disabled buttons!");
  }
  newWindow.close();
  finish();
}
