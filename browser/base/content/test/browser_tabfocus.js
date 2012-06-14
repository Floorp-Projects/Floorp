/*
 * This test checks that focus is adjusted properly when switching tabs.
 */

let testPage1 = "data:text/html,<html id='tab1'><body><button id='button1'>Tab 1</button></body></html>";
let testPage2 = "data:text/html,<html id='tab2'><body><button id='button2'>Tab 2</button></body></html>";
let testPage3 = "data:text/html,<html id='tab3'><body><button id='button3'>Tab 3</button></body></html>";

function test() {
  waitForExplicitFinish();

  var tab1 = gBrowser.addTab();
  var browser1 = gBrowser.getBrowserForTab(tab1);

  var tab2 = gBrowser.addTab();
  var browser2 = gBrowser.getBrowserForTab(tab2);

  gURLBar.focus();

  var loadCount = 0;
  function check()
  {
    // wait for both tabs to load
    if (++loadCount != 2)
      return;

    browser1.removeEventListener("load", check, true);
    browser2.removeEventListener("load", check, true);
    executeSoon(_run_focus_tests);
  }

  function _run_focus_tests() {
    window.focus();

    _browser_tabfocus_test_lastfocus = gURLBar;
    _browser_tabfocus_test_lastfocuswindow = window;

    window.addEventListener("focus", _browser_tabfocus_test_eventOccured, true);
    window.addEventListener("blur", _browser_tabfocus_test_eventOccured, true);

    // make sure that the focus initially starts out blank
    var fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
    var focusedWindow = {};
    is(fm.getFocusedElementForWindow(browser1.contentWindow, false, focusedWindow), null, "initial focus in tab 1");
    is(focusedWindow.value, browser1.contentWindow, "initial frame focus in tab 1");
    is(fm.getFocusedElementForWindow(browser2.contentWindow, false, focusedWindow), null, "initial focus in tab 2");
    is(focusedWindow.value, browser2.contentWindow, "initial frame focus in tab 2");

    expectFocusShift(function () gBrowser.selectedTab = tab2,
                     browser2.contentWindow, null, true,
                     "focusedElement after tab change, focus in new tab");

    // switching tabs when nothing in the new tab is focused
    // should focus the browser
    expectFocusShift(function () gBrowser.selectedTab = tab1,
                     browser1.contentWindow, null, true,
                     "focusedElement after tab change, focus in new tab");

    // focusing a button in the current tab should focus it
    var button1 = browser1.contentDocument.getElementById("button1");
    expectFocusShift(function () button1.focus(),
                     browser1.contentWindow, button1, true,
                     "focusedWindow after focus in focused tab");

    // focusing a button in a background tab should not change the actual
    // focus, but should set the focus that would be in that background tab to
    // that button.
    var button2 = browser2.contentDocument.getElementById("button2");
    button2.focus();

    expectFocusShift(function () button2.focus(),
                     browser1.contentWindow, button1, false,
                     "focusedWindow after focus in unfocused tab");
    is(fm.getFocusedElementForWindow(browser2.contentWindow, false, {}), button2, "focus in unfocused tab");

    // switching tabs should now make the button in the other tab focused
    expectFocusShift(function () gBrowser.selectedTab = tab2,
                     browser2.contentWindow, button2, true,
                     "focusedWindow after tab change");

    // blurring an element in a background tab should not change the active
    // focus, but should clear the focus in that tab.
    expectFocusShift(function () button1.blur(),
                     browser2.contentWindow, button2, false,
                     "focusedWindow after blur in unfocused tab");
    is(fm.getFocusedElementForWindow(browser1.contentWindow, false, {}), null, "blur in unfocused tab");

    // When focus is in the tab bar, it should be retained there
    expectFocusShift(function () gBrowser.selectedTab.focus(),
                     window, gBrowser.selectedTab, true,
                     "focusing tab element");
    expectFocusShift(function () gBrowser.selectedTab = tab1,
                     window, tab1, true,
                     "tab change when selected tab element was focused");
    expectFocusShift(function () gBrowser.selectedTab = tab2,
                     window, tab2, true,
                     "tab change when selected tab element was focused");
    expectFocusShift(function () gBrowser.selectedTab.blur(),
                     window, null, true,
                     "blurring tab element");

    // focusing the url field should switch active focus away from the browser but
    // not clear what would be the focus in the browser
    button1.focus();
    expectFocusShift(function () gURLBar.focus(),
                     window, gURLBar.inputField, true,
                     "focusedWindow after url field focused");
    is(fm.getFocusedElementForWindow(browser2.contentWindow, false, {}), button2, "url field focused, button in browser");
    expectFocusShift(function () gURLBar.blur(),
                     window, null, true,
                     "blurring url field");

    // when a chrome element is focused, switching tabs to a tab with a button
    // with the current focus should focus the button
    expectFocusShift(function () gBrowser.selectedTab = tab1,
                     browser1.contentWindow, button1, true,
                     "focusedWindow after tab change, focus in url field, button focused in new tab");
    is(fm.getFocusedElementForWindow(browser2.contentWindow, false, {}), button2, "after switch tab, focus in unfocused tab");

    // blurring an element in the current tab should clear the active focus
    expectFocusShift(function () button1.blur(),
                     browser1.contentWindow, null, true,
                     "focusedWindow after blur in focused tab");

    // blurring an non-focused url field should have no effect
    expectFocusShift(function () gURLBar.blur(),
                     browser1.contentWindow, null, false,
                     "focusedWindow after blur in unfocused url field");

    // switch focus to a tab with a currently focused element
    expectFocusShift(function () gBrowser.selectedTab = tab2,
                     browser2.contentWindow, button2, true,
                     "focusedWindow after switch from unfocused to focused tab");

    // clearing focus on the chrome window should switch the focus to the
    // chrome window
    expectFocusShift(function () fm.clearFocus(window),
                     window, null, true,
                     "focusedWindow after switch to chrome with no focused element");

    // switch focus to another tab when neither have an active focus
    expectFocusShift(function () gBrowser.selectedTab = tab1,
                     browser1.contentWindow, null, true,
                     "focusedWindow after tab switch from no focus to no focus");

    gURLBar.focus();
    _browser_tabfocus_test_events = "";
    _browser_tabfocus_test_lastfocus = gURLBar;
    _browser_tabfocus_test_lastfocuswindow = window;

    expectFocusShift(function () EventUtils.synthesizeKey("VK_F6", { }),
                     browser1.contentWindow, browser1.contentDocument.documentElement,
                     true, "switch document forward with f6");
    EventUtils.synthesizeKey("VK_F6", { });
    is(fm.focusedWindow, window, "switch document forward again with f6");

    browser1.style.MozUserFocus = "ignore";
    browser1.clientWidth;
    EventUtils.synthesizeKey("VK_F6", { });
    is(fm.focusedWindow, window, "switch document forward again with f6 when browser non-focusable");

    window.removeEventListener("focus", _browser_tabfocus_test_eventOccured, true);
    window.removeEventListener("blur", _browser_tabfocus_test_eventOccured, true);

    // next, check whether navigating forward, focusing the urlbar and then
    // navigating back maintains the focus in the urlbar.
    browser1.addEventListener("pageshow", _browser_tabfocus_navigation_test_eventOccured, true);
    button1.focus();
    browser1.contentWindow.location = testPage3;
  }

  browser1.addEventListener("load", check, true);
  browser2.addEventListener("load", check, true);
  browser1.contentWindow.location = testPage1;
  browser2.contentWindow.location = testPage2;
}

var _browser_tabfocus_test_lastfocus;
var _browser_tabfocus_test_lastfocuswindow = null;
var _browser_tabfocus_test_events = "";

function _browser_tabfocus_test_eventOccured(event)
{
  var id;
  if (event.target instanceof Window)
    id = event.originalTarget.document.documentElement.id + "-window";
  else if (event.target instanceof Document)
    id = event.originalTarget.documentElement.id + "-document";
  else if (event.target.id == "urlbar" && event.originalTarget.localName == "input")
    id = "urlbar";
  else
    id = event.originalTarget.id;

  if (_browser_tabfocus_test_events)
    _browser_tabfocus_test_events += " ";
  _browser_tabfocus_test_events += event.type + ": " + id;
}

function _browser_tabfocus_navigation_test_eventOccured(event)
{
  if (event.target instanceof Document) {
    var contentwin = event.target.defaultView;
    if (contentwin.location.toString().indexOf("3") > 0) {
      // just moved forward, so focus the urlbar and go back
      gURLBar.focus();
      setTimeout(function () contentwin.history.back(), 0);
    }
    else if (contentwin.location.toString().indexOf("2") > 0) {
      event.currentTarget.removeEventListener("pageshow", _browser_tabfocus_navigation_test_eventOccured, true);
      is(window.document.activeElement, gURLBar.inputField, "urlbar still focused after navigating back");
      gBrowser.removeCurrentTab();
      gBrowser.removeCurrentTab();
      finish();
    }
  }
}

function getId(element)
{
  return (element.localName == "input") ? "urlbar" : element.id;
}

function expectFocusShift(callback, expectedWindow, expectedElement, focusChanged, testid)
{
  var expectedEvents = "";
  if (focusChanged) {
    if (_browser_tabfocus_test_lastfocus)
      expectedEvents += "blur: " + getId(_browser_tabfocus_test_lastfocus);

    if (_browser_tabfocus_test_lastfocuswindow &&
        _browser_tabfocus_test_lastfocuswindow != expectedWindow) {
      if (expectedEvents)
        expectedEvents += " ";
      var windowid = _browser_tabfocus_test_lastfocuswindow.document.documentElement.id;
      expectedEvents += "blur: " + windowid + "-document " +
                        "blur: " + windowid + "-window";
    }

    if (expectedWindow && _browser_tabfocus_test_lastfocuswindow != expectedWindow) {
      if (expectedEvents)
        expectedEvents += " ";
      var windowid = expectedWindow.document.documentElement.id;
      expectedEvents += "focus: " + windowid + "-document " +
                        "focus: " + windowid + "-window";
    }

    if (expectedElement && expectedElement != expectedElement.ownerDocument.documentElement) {
      if (expectedEvents)
        expectedEvents += " ";
      expectedEvents += "focus: " + getId(expectedElement);
    }

    _browser_tabfocus_test_lastfocus = expectedElement;
    _browser_tabfocus_test_lastfocuswindow = expectedWindow;
  }

  callback();

  is(_browser_tabfocus_test_events, expectedEvents, testid + " events");
  _browser_tabfocus_test_events = "";

  var fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

  var focusedElement = fm.focusedElement;
  is(focusedElement ? getId(focusedElement) : "none",
     expectedElement ? getId(expectedElement) : "none", testid + " focusedElement");
  is(fm.focusedWindow, expectedWindow, testid + " focusedWindow");
  var focusedWindow = {};
  is(fm.getFocusedElementForWindow(expectedWindow, false, focusedWindow),
     expectedElement, testid + " getFocusedElementForWindow");
  is(focusedWindow.value, expectedWindow, testid + " getFocusedElementForWindow frame");
  is(expectedWindow.document.hasFocus(), true, testid + " hasFocus");
  var expectedActive = expectedElement;
  if (!expectedActive)
    expectedActive = expectedWindow.document instanceof XULDocument ?
                     expectedWindow.document.documentElement : expectedWindow.document.body;
  is(expectedWindow.document.activeElement, expectedActive, testid + " activeElement");
}
