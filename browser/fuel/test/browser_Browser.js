const Ci = Components.interfaces;
const Cc = Components.classes;

function url(spec) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  return ios.newURI(spec, null, null);
}

var gPageA = null;
var gPageB = null;

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

  gPageA = activeWin.open(url("chrome://mochikit/content/browser/browser/fuel/test/ContentA.html"));
  gPageA.events.addListener("load", onPageAFirstLoad);

  is(activeWin.tabs.length, 2, "Checking length of 'Browser.tabs' after opening 1 additional tab");

  waitForExplicitFinish();

  function onPageAFirstLoad(event) {
    gPageA.events.removeListener("load", onPageAFirstLoad);

    gPageB = activeWin.open(url("chrome://mochikit/content/browser/browser/fuel/test/ContentB.html"));
    gPageB.events.addListener("load", afterOpen);
    gPageB.focus();

    is(activeWin.tabs.length, 3, "Checking length of 'Browser.tabs' after opening a second additional tab");
    is(activeWin.activeTab.index, gPageB.index, "Checking 'Browser.activeTab' after setting focus");
  }

  // need to wait for the url's to be refreshed during the load
  function afterOpen(event) {
    gPageB.events.removeListener("load", afterOpen);

    is(gPageA.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentA.html", "Checking 'BrowserTab.uri' after opening");
    is(gPageB.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentB.html", "Checking 'BrowserTab.uri' after opening");

    // check event
    is(gTabOpenCount, 2, "Checking event handler for tab open");

    // test document access
    var test1 = gPageA.document.getElementById("test1");
    ok(test1, "Checking existence of element in content DOM");
    is(test1.innerHTML, "A", "Checking content of element in content DOM");

    // test moving tab
    gPageA.moveToEnd();
    is(gPageA.index, 2, "Checking index after moving tab");

    // check event
    is(gTabMoveCount, 1, "Checking event handler for tab move");

    // test loading new content into a tab
    // the event will be checked in onPageLoad
    gPageA.events.addListener("load", onPageASecondLoad);
    gPageA.load(gPageB.uri);

    // test loading new content with a frame into a tab
    // the event will be checked in afterClose
    gPageB.events.addListener("load", onPageBLoadWithFrames);
    gPageB.load(url("chrome://mochikit/content/browser/browser/fuel/test/ContentWithFrames.html"));
  }

  function onPageASecondLoad(event) {
    is(gPageA.uri.spec, "chrome://mochikit/content/browser/browser/fuel/test/ContentB.html", "Checking 'BrowserTab.uri' after loading new content");

    // start testing closing tabs
    // the event will be checked in afterClose
    // use executeSoon so the onPageASecondLoad
    // has a chance to finish first
    executeSoon(function() {
      gPageA.close();
      gPageB.close();
      executeSoon(afterClose);
     });
  }

  function onPageBLoadWithFrames(event) {
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
