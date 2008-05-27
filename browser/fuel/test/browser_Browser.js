const Ci = Components.interfaces;
const Cc = Components.classes;

function url(spec) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  return ios.newURI(spec, null, null);
}

var gTabOpenCount = 0;
var gTabCloseCount = 0;
var gTabMoveCount = 0;
var gPageLoadCount = 0;

function test() {
  var windows = Application.windows;
  ok(windows, "Check access to browser windows");
  ok(windows.length, "There should be at least one browser window open");

  var activeWin = Application.activeWindow;
  activeWin.events.addListener("TabOpen", onTabOpen);
  activeWin.events.addListener("TabClose", onTabClose);
  activeWin.events.addListener("TabMove", onTabMove);

  var pageA = activeWin.open(url("chrome://mochikit/content/browser/browser/fuel/test/ContentA.html"));
  var pageB = activeWin.open(url("chrome://mochikit/content/browser/browser/fuel/test/ContentB.html"));
  pageB.focus();

  is(activeWin.tabs.length, 3, "Checking length of 'Browser.tabs' after opening 2 additional tabs");
  is(activeWin.activeTab.index, pageB.index, "Checking 'Browser.activeTab' after setting focus");

  waitForExplicitFinish();
  setTimeout(afterOpen, 1000);

  // need to wait for the url's to be refreshed during the load
  function afterOpen() {
    is(pageA.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentA.html", "Checking 'BrowserTab.uri' after opening");
    is(pageB.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentB.html", "Checking 'BrowserTab.uri' after opening");

    // check event
    is(gTabOpenCount, 2, "Checking event handler for tab open");

    // test document access
    var test1 = pageA.document.getElementById("test1");
    ok(test1, "Checking existence of element in content DOM");
    is(test1.innerHTML, "A", "Checking content of element in content DOM");

    // test moving tab
    pageA.moveToEnd();
    is(pageA.index, 2, "Checking index after moving tab");

    // check event
    is(gTabMoveCount, 1, "Checking event handler for tab move");

    // test loading new content into a tab
    // the event will be checked in onPageLoad
    pageA.events.addListener("load", onPageLoad);
    pageA.load(pageB.uri);

    // test loading new content with a frame into a tab
    // the event will be checked in afterClose
    pageB.events.addListener("load", onPageLoadWithFrames);
    pageB.load(url("chrome://mochikit/content/browser/browser/fuel/test/ContentWithFrames.html"));
  }

  function onPageLoad(event) {
    is(pageA.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentB.html", "Checking 'BrowserTab.uri' after loading new content");

    // start testing closing tabs
    // the event will be checked in afterClose
    // use a setTimeout so the pageloadwithframes
    // has a chance to finish first
    setTimeout(function() {
      pageA.close();
      pageB.close();
      setTimeout(afterClose, 1000);
     }, 1000);
  }

  function onPageLoadWithFrames(event) {
    gPageLoadCount++;
  }
  
  function afterClose() {
    // check close event
    is(gTabCloseCount, 2, "Checking event handler for tab close");

    // check page load with frame event
    is(gPageLoadCount, 1, "Checking 'BrowserTab.uri' after loading new content with a frame");

    is(activeWin.tabs.length, 1, "Checking length of 'Browser.tabs' after closing 2 tabs");

    finish();
  }
}

function onTabOpen(event) {
  gTabOpenCount++;
}

function onTabClose(event) {
  gTabCloseCount++;
}

function onTabMove(event) {
  gTabMoveCount++;
}
