const TEST_URL_BASES = [
  "http://example.org/browser/browser/base/content/test/dummy_page.html#tabmatch",
  "http://example.org/browser/browser/base/content/test/moz.png#tabmatch"
];

var gIOService = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
var gWindowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                        getService(Ci.nsIWindowMediator);
var gPrivateBrowsing = Cc["@mozilla.org/privatebrowsing;1"].
                         getService(Ci.nsIPrivateBrowsingService);
var gObserverService = Cc["@mozilla.org/observer-service;1"].
                         getService(Ci.nsIObserverService);


var gTabWaitCount = 0;
var gTabCounter = 0;
var gNewWindow = null;

var gTestSteps = [
  function() {
    for (let i = 0; i < 10; i++) {
      let tab = gBrowser.addTab();
      loadTab(tab, TEST_URL_BASES[0] + (++gTabCounter));
    }
  },
  function() {
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    for (let i = 1; i < gBrowser.mTabs.length; i++)
      loadTab(gBrowser.mTabs[i], TEST_URL_BASES[1] + (++gTabCounter));
  },
  function() {
    for (let i = 1; i < gBrowser.mTabs.length; i++)
      loadTab(gBrowser.mTabs[i], TEST_URL_BASES[0] + gTabCounter);
  },
  function() {
    gNewWindow = openNewWindowWith("about:blank");
    gNewWindow.addEventListener("load", function () {
      gNewWindow.removeEventListener("load", arguments.callee, false);
      var delayedStartup = gNewWindow.delayedStartup;
      gNewWindow.delayedStartup = function() {
        delayedStartup.apply(gNewWindow, arguments);
        loadTab(gNewWindow.gBrowser.mTabs[0], TEST_URL_BASES[0] + gTabCounter);
      }
    }, false);
  },
  function() {
    gPrefService.setBoolPref("browser.tabs.warnOnClose", false);
    gNewWindow.close();
    if (gPrefService.prefHasUserValue("browser.tabs.warnOnClose"))
      gPrefService.clearUserPref("browser.tabs.warnOnClose");
    ensure_opentabs_match_db();
    executeSoon(nextStep);
  },
  function() {
    gPrivateBrowsing.privateBrowsingEnabled = true;

    executeSoon(function() {
      ensure_opentabs_match_db();
      nextStep();
    });
  },
  function() {
    gPrivateBrowsing.privateBrowsingEnabled = false;
    setTimeout(function() {
      ensure_opentabs_match_db();
      nextStep();
    }, 100);
  }
];



function test() {
  waitForExplicitFinish();
  nextStep();
}

function loadTab(tab, url) {
  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);

    if (--gTabWaitCount > 0)
      return;
    is(gTabWaitCount, 0,
       "sanity check, gTabWaitCount should not be decremented below 0");

    try {
      ensure_opentabs_match_db();
    } catch (e) {
      ok(false, "exception from ensure_openpages_match_db: " + e);
    }

    executeSoon(nextStep);
  }, true);
  gTabWaitCount++;
  tab.linkedBrowser.loadURI(url);
}

function nextStep() {
  if (gTestSteps.length == 0) {
    while (gBrowser.mTabs.length > 1) {
      gBrowser.selectTabAtIndex(1);
      gBrowser.removeCurrentTab();
    }
    PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
    finish();
    return;
  }

  var stepFunc = gTestSteps.shift();
  stepFunc();
}

function ensure_opentabs_match_db() {
  var tabs = {};

  var winEnum = gWindowMediator.getEnumerator("navigator:browser");
  while (winEnum.hasMoreElements()) {
    let browserWin = winEnum.getNext();
    // skip closed-but-not-destroyed windows
    if (browserWin.closed)
      continue;

    for (let i = 0; i < browserWin.gBrowser.mTabContainer.childElementCount; i++) {
      let browser = browserWin.gBrowser.getBrowserAtIndex(i);
      let url = browser.currentURI.spec;
      if (!(url in tabs))
        tabs[url] = 1;
      else
        tabs[url]++;
    }
  }

  var db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;

  try {
    var stmt = db.createStatement(
                          "SELECT IFNULL(p_t.url, p.url) AS url, open_count, place_id " +
                          "FROM moz_openpages_temp " +
                          "LEFT JOIN moz_places p ON p.id=place_id " +
                          "LEFT JOIN moz_places_temp p_t ON p_t.id=place_id");
  } catch (e) {
    ok(false, "error creating db statement: " + e);
    return;
  }

  var dbtabs = [];
  try {
    while (stmt.executeStep()) {
      ok(stmt.row.url in tabs,
         "url is in db, should be in tab: " + stmt.row.url);
      is(tabs[stmt.row.url], stmt.row.open_count,
         "db count (" + stmt.row.open_count + ") " +
         "should match actual open tab count " +
         "(" + tabs[stmt.row.url] + "): " + stmt.row.url);
      dbtabs.push(stmt.row.url);
    }
  } finally {
    stmt.finalize();
  }

  for (let url in tabs) {
    // ignore URLs that should never be in the places db
    if (!is_expected_in_db(url))
      continue;
    ok(dbtabs.indexOf(url) > -1,
       "tab is open (" + tabs[url] + " times) and should recorded in db: " + url);
  }
}

function is_expected_in_db(url) {
  var uri = gIOService.newURI(url, null, null);
  return PlacesUtils.history.canAddURI(uri);
}
