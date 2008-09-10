/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dietrich Ayala <dietrich@mozilla.com>
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

/*

Sets up the database for subsequent performance
tests, and test the speed of adding to history
and bookmarks.

- add XXX visits distributed over XXX days
- add XXX bookmarks distributed over XXX days

*/

/*********************** begin header **********************/
waitForExplicitFinish();

const TEST_IDENTIFIER = "ui-perf-test";
const TEST_SUITE = "places";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
         getService(Ci.nsIWindowMediator);
var win = wm.getMostRecentWindow("navigator:browser");

var ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

function add_visit(aURI, aDate) {
  var placeID = hs.addVisit(aURI,
                            aDate,
                            null, // no referrer
                            hs.TRANSITION_TYPED, // user typed in URL bar
                            false, // not redirect
                            0);
  return placeID;
}

function add_bookmark(aURI) {
  var bId = bs.insertBookmark(bs.unfiledBookmarksFolder, aURI,
                              bs.DEFAULT_INDEX, "bookmark/" + aURI.spec);
  return bId;
}

function make_test_report(testName, result, units) {
  return [TEST_IDENTIFIER, TEST_SUITE, testName, result, units||"ms"].join(":");
}

// Each test is an obj w/ a name property and run method
var ptests = [];

/*********************** end header **********************/

// add visits and bookmarks
ptests.push({
  run: function() {
    bs.runInBatchMode({
      runBatched: function(aUserData) {
        // timespan - same as default history pref for now
        var days = 90;

        // add visits, distributed across the timespan
        var total_visits = 300;
        var visits_per_day = total_visits/days;

        var visit_date_microsec = Date.now() * 1000;
        var day_counter = 0;
        
        var start = Date.now();
        for (var i = 0; i < days; i++) {
          visit_date_microsec -= 86400 * 1000 * 1000; // remove a day
          var spec = "http://example.com/" + visit_date_microsec;
          for (var j = 0; j < visits_per_day; j++) {
            var uri = ios.newURI(spec + j, null, null);
            add_visit(uri, visit_date_microsec);
          }
        }
        var duration = Date.now() - start;
        var report = make_test_report("add_visits", duration);
        ok(true, report);

        // add bookmarks
        var bookmarks_total = total_visits/10; // bookmark a tenth of the URLs in history
        var bookmarks_per_day = bookmarks_total/days;

        // reset visit date counter
        visit_date_microsec = Date.now() * 1000;
        var bookmark_counter = 0;
        start = Date.now();
        for (var i = 0; i < days; i++) {
          visit_date_microsec -= 86400 * 1000 * 1000; // remove a day
          var spec = "http://example.com/" + visit_date_microsec;
          for (var j = 0; j < visits_per_day; j++) {
            var uri = ios.newURI(spec + j, null, null);
            if (bookmark_counter < bookmarks_per_day) {
              add_bookmark(uri);
              bookmark_counter++;
            }
            else
              bookmark_counter = 0;
          }
        }
        duration = Date.now() - start;
        report = make_test_report("add_bookmarks", duration);
        ok(true, report);
        runNextTest();
      }
    }, null);
  }
});

function test() {
  // kick off tests
  runNextTest();
}

function runNextTest() {
  if (ptests.length > 0)
    ptests.shift().run();
  else
    finish();
}
