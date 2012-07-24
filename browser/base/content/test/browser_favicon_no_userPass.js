var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gIOService = Cc["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService);
var gFaviconService = Cc["@mozilla.org/browser/favicon-service;1"]
                      .getService(Ci.nsIFaviconService);
var gTestBrowser = null;
var gNextTest = null;
var gOkayIfIconChanges = false;

Components.utils.import("resource://gre/modules/Services.jsm");

var gHistoryService = Cc["@mozilla.org/browser/nav-history-service;1"]
                      .getService(Ci.nsINavHistoryService);

var gHistoryObserver = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {},
  onTitleChanged: function(aURI, aPageTitle) {},
  onBeforeDeleteURI: function(aURI) {},
  onDeleteURI: function(aURI) {},
  onClearHistory: function() {},
  onDeleteVisits: function() {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver]),

  onPageChanged: function(aURI, aWhat, aValue) {
    if (aWhat == Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
      if (gOkayIfIconChanges) {
        ok(true, "favicon changed");
        finishTest();
      }
      else {
        ok(false, "favicon should not have changed now");
        finishTest();
      }
    }
  }
};

gHistoryService.addObserver(gHistoryObserver, false);

function test() {
  waitForExplicitFinish();

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gBrowser.selectedTab.addEventListener("error", part2, true);
  prepareTest(stub1, gTestRoot + "favicon_no_userPass.html#link");
}

function pageLoad() {
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

function stub1() {
  gNextTest = null;
}

function part2() {
  ok(true, "got expected error for loading favicon");
  gBrowser.selectedTab.removeEventListener("error", part2, true);
  prepareTest(part3, gTestRoot + "favicon_no_userPass.html#img");
  gTestBrowser.reload();
}

function part3() {
  gBrowser.selectedTab.addEventListener("error", stub2, true);
  gOkayIfIconChanges = true;
  prepareTest(part4, gTestRoot + "favicon_no_userPass.html#link2");
  gTestBrowser.reload();
}

function part4() {
  gNextTest = null;
}

function stub2() {
  ok(false, "got unexpected error for loading favicon the second time");
  finishTest();
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gHistoryService.removeObserver(gHistoryObserver);
  gBrowser.selectedTab.removeEventListener("error", stub2, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}
