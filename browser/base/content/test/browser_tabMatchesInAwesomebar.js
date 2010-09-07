/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blair McBride <bmcbride@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const TEST_URL_BASES = [
  "http://example.org/browser/browser/base/content/test/dummy_page.html#tabmatch",
  "http://example.org/browser/browser/base/content/test/moz.png#tabmatch"
];

var gPrivateBrowsing = Cc["@mozilla.org/privatebrowsing;1"].
                         getService(Ci.nsIPrivateBrowsingService);


var gTabWaitCount = 0;
var gTabCounter = 0;

var gTestSteps = [
  function() {
    info("Running step 1");
    for (let i = 0; i < 10; i++) {
      let tab = gBrowser.addTab();
      loadTab(tab, TEST_URL_BASES[0] + (++gTabCounter));
    }
  },
  function() {
    info("Running step 2");
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    for (let i = 1; i < gBrowser.tabs.length; i++)
      loadTab(gBrowser.tabs[i], TEST_URL_BASES[1] + (++gTabCounter));
  },
  function() {
    info("Running step 3");
    for (let i = 1; i < gBrowser.tabs.length; i++)
      loadTab(gBrowser.tabs[i], TEST_URL_BASES[0] + gTabCounter);
  },
  function() {
    info("Running step 4");
    let ps = Services.prefs;
    ps.setBoolPref("browser.privatebrowsing.keep_current_session", true);
    ps.setBoolPref("browser.tabs.warnOnClose", false);

    gPrivateBrowsing.privateBrowsingEnabled = true;

    executeSoon(function() {
      ensure_opentabs_match_db();
      nextStep();
    });
  },
  function() {
    info("Running step 5");
    gPrivateBrowsing.privateBrowsingEnabled = false;

    executeSoon(function() {
      let ps = Services.prefs;
      try {
        ps.clearUserPref("browser.privatebrowsing.keep_current_session");
      } catch (ex) {}
      try {
        ps.clearUserPref("browser.tabs.warnOnClose");
      } catch (ex) {}

      ensure_opentabs_match_db();
      nextStep()
    });
  },
  function() {
    info("Running step 6 - ensure we don't register subframes as open pages");
    let tab = gBrowser.addTab();
    tab.linkedBrowser.addEventListener("load", function () {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      // Start the sub-document load.
      executeSoon(function () {
        tab.linkedBrowser.addEventListener("load", function (e) {
          tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
            ensure_opentabs_match_db();
            nextStep()
        }, true);
        tab.linkedBrowser.contentDocument.querySelector("iframe").src = "http://test2.example.org/";
      });
    }, true);
    tab.linkedBrowser.loadURI('data:text/html,<body><iframe src=""></iframe></body>');
  },
  function() {
    info("Running step 7 - remove tab immediately");
    let tab = gBrowser.addTab("about:blank");
    gBrowser.removeTab(tab);
    ensure_opentabs_match_db();
    nextStep();
  },
  function() {
    info("Running step 8 - check swapBrowsersAndCloseOther preserves registered switch-to-tab result");
    let tabToKeep = gBrowser.addTab();
    let tab = gBrowser.addTab();
    tab.linkedBrowser.addEventListener("load", function () {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      gBrowser.swapBrowsersAndCloseOther(tabToKeep, tab);
      ensure_opentabs_match_db();
      gBrowser.removeTab(tabToKeep);
      ensure_opentabs_match_db();
      nextStep();
    }, true);
    tab.linkedBrowser.loadURI('about:robots');
  },
];



function test() {
  waitForExplicitFinish();
  nextStep();
}

function loadTab(tab, url) {
  // Because adding visits is async, we will not be notified immediately.
  let visited = false;

  tab.linkedBrowser.addEventListener("load", function (event) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    Services.obs.addObserver(
      function (aSubject, aTopic, aData) {
        if (url != aSubject.QueryInterface(Ci.nsIURI).spec)
          return;
        Services.obs.removeObserver(arguments.callee, aTopic);
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
      }, "uri-visit-saved", false);
  }, true);

  gTabWaitCount++;
  tab.linkedBrowser.loadURI(url);
}

function nextStep() {
  if (gTestSteps.length == 0) {
    while (gBrowser.tabs.length > 1) {
      gBrowser.selectTabAtIndex(1);
      gBrowser.removeCurrentTab();
    }

    waitForClearHistory(finish);

    return;
  }

  var stepFunc = gTestSteps.shift();
  stepFunc();
}

function ensure_opentabs_match_db() {
  var tabs = {};

  var winEnum = Services.wm.getEnumerator("navigator:browser");
  while (winEnum.hasMoreElements()) {
    let browserWin = winEnum.getNext();
    // skip closed-but-not-destroyed windows
    if (browserWin.closed)
      continue;

    for (let i = 0; i < browserWin.gBrowser.tabContainer.childElementCount; i++) {
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
                          "SELECT t.url, open_count, IFNULL(p_t.id, p.id) " +
                          "FROM moz_openpages_temp t " +
                          "LEFT JOIN moz_places p ON p.url = t.url " +
                          "LEFT JOIN moz_places_temp p_t ON p_t.url = t.url");
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
    ok(dbtabs.indexOf(url) > -1,
       "tab is open (" + tabs[url] + " times) and should recorded in db: " + url);
  }
}

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
}
