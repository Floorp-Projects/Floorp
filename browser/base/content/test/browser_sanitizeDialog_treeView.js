/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is sanitize dialog test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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

/**
 * Tests the sanitize dialog (a.k.a. the clear recent history dialog).
 * See bug 480169.
 *
 * The purpose of this test is not to fully flex the sanitize timespan code;
 * browser/base/content/test/browser_sanitize-timespans.js does that.  This
 * test checks the UI of the dialog and makes sure it's correctly connected to
 * the sanitize timespan code.
 *
 * Some of this code, especially the history creation parts, was taken from
 * browser/base/content/test/browser_sanitize-timespans.js.
 */

Cc["@mozilla.org/moz/jssubscript-loader;1"].
  getService(Ci.mozIJSSubScriptLoader).
  loadSubScript("chrome://browser/content/sanitize.js");

const dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
const bhist = Cc["@mozilla.org/browser/global-history;2"].
              getService(Ci.nsIBrowserHistory);
const formhist = Cc["@mozilla.org/satchel/form-history;1"].
                 getService(Ci.nsIFormHistory2);

// Add tests here.  Each is a function that's called by doNextTest().
var gAllTests = [

  /**
   * Moves the grippy around, makes sure it works OK.
   */
  function () {
    // Add history (within the past hour) to get some rows in the tree.
    let uris = [];
    for (let i = 0; i < 30; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }

    // Open the dialog and do our tests.
    openWindow(function (aWin) {
      let wh = new WindowHelper(aWin);
      wh.selectDuration(Sanitizer.TIMESPAN_HOUR);
      wh.checkGrippy("Grippy should be at last row after selecting HOUR " +
                     "duration",
                     wh.getRowCount() - 1);

      // Move the grippy around.
      let row = wh.getGrippyRow();
      while (row !== 0) {
        row--;
        wh.moveGrippyBy(-1);
        wh.checkGrippy("Grippy should be moved up one row", row);
      }
      wh.moveGrippyBy(-1);
      wh.checkGrippy("Grippy should remain at first row after trying to move " +
                     "it up",
                     0);
      while (row !== wh.getRowCount() - 1) {
        row++;
        wh.moveGrippyBy(1);
        wh.checkGrippy("Grippy should be moved down one row", row);
      }
      wh.moveGrippyBy(1);
      wh.checkGrippy("Grippy should remain at last row after trying to move " +
                     "it down",
                     wh.getRowCount() - 1);

      // Cancel the dialog, make sure history visits are not cleared.
      wh.checkPrefCheckbox("history", false);

      wh.cancelDialog();
      ensureHistoryClearedState(uris, false);

      // OK, done, cleanup after ourselves.
      blankSlate();
      ensureHistoryClearedState(uris, true);
    });
  },

  /**
   * Ensures that the combined history-downloads checkbox clears both history
   * visits and downloads when checked; the dialog respects simple timespan.
   */
  function () {
    // Add history and downloads (within the past hour).
    let uris = [];
    for (let i = 0; i < 30; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }
    let downloadIDs = [];
    for (let i = 0; i < 5; i++) {
      downloadIDs.push(addDownloadWithMinutesAgo(i));
    }
    // Add history and downloads (over an hour ago).
    let olderURIs = [];
    for (let i = 0; i < 5; i++) {
      olderURIs.push(addHistoryWithMinutesAgo(61 + i));
    }
    let olderDownloadIDs = [];
    for (let i = 0; i < 5; i++) {
      olderDownloadIDs.push(addDownloadWithMinutesAgo(61 + i));
    }
    let totalHistoryVisits = uris.length + olderURIs.length;

    // Open the dialog and do our tests.
    openWindow(function (aWin) {
      let wh = new WindowHelper(aWin);
      wh.selectDuration(Sanitizer.TIMESPAN_HOUR);
      wh.checkGrippy("Grippy should be at proper row after selecting HOUR " +
                     "duration",
                     uris.length);

      // Accept the dialog, make sure history visits and downloads within one
      // hour are cleared.
      wh.checkPrefCheckbox("history", true);
      wh.acceptDialog();
      ensureHistoryClearedState(uris, true);
      ensureDownloadsClearedState(downloadIDs, true);

      // Make sure visits and downloads > 1 hour still exist.
      ensureHistoryClearedState(olderURIs, false);
      ensureDownloadsClearedState(olderDownloadIDs, false);

      // OK, done, cleanup after ourselves.
      blankSlate();
      ensureHistoryClearedState(olderURIs, true);
      ensureDownloadsClearedState(olderDownloadIDs, true);
    });
  },

  /**
   * Ensures that the combined history-downloads checkbox removes neither
   * history visits nor downloads when not checked.
   */
  function () {
    // Add history, downloads, form entries (within the past hour).
    let uris = [];
    for (let i = 0; i < 5; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }
    let downloadIDs = [];
    for (let i = 0; i < 5; i++) {
      downloadIDs.push(addDownloadWithMinutesAgo(i));
    }
    let formEntries = [];
    for (let i = 0; i < 5; i++) {
      formEntries.push(addFormEntryWithMinutesAgo(i));
    }

    // Open the dialog and do our tests.
    openWindow(function (aWin) {
      let wh = new WindowHelper(aWin);
      wh.selectDuration(Sanitizer.TIMESPAN_HOUR);
      wh.checkGrippy("Grippy should be at last row after selecting HOUR " +
                     "duration",
                     wh.getRowCount() - 1);

      // Remove only form entries, leave history (including downloads).
      wh.checkPrefCheckbox("history", false);
      wh.checkPrefCheckbox("formdata", true);
      wh.acceptDialog();

      // Of the three only form entries should be cleared.
      ensureHistoryClearedState(uris, false);
      ensureDownloadsClearedState(downloadIDs, false);
      ensureFormEntriesClearedState(formEntries, true);

      // OK, done, cleanup after ourselves.
      blankSlate();
      ensureHistoryClearedState(uris, true);
      ensureDownloadsClearedState(downloadIDs, true);
    });
  },

  /**
   * Ensures that the "Everything" duration option works.
   */
  function () {
    // Add history.
    let uris = [];
    uris.push(addHistoryWithMinutesAgo(10));  // within past hour
    uris.push(addHistoryWithMinutesAgo(70));  // within past two hours
    uris.push(addHistoryWithMinutesAgo(130)); // within past four hours
    uris.push(addHistoryWithMinutesAgo(250)); // outside past four hours

    // Open the dialog and do our tests.
    openWindow(function (aWin) {
      let wh = new WindowHelper(aWin);
      wh.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
      wh.checkPrefCheckbox("history", true);
      wh.acceptDialog();
      ensureHistoryClearedState(uris, true);
    });
  }
];

// Used as the download database ID for a new download.  Incremented for each
// new download.  See addDownloadWithMinutesAgo().
var gDownloadId = 5555551;

// Index in gAllTests of the test currently being run.  Incremented for each
// test run.  See doNextTest().
var gCurrTest = 0;

var now_uSec = Date.now() * 1000;

///////////////////////////////////////////////////////////////////////////////

/**
 * This wraps the dialog and provides some convenience methods for interacting
 * with it.
 *
 * A warning:  Before you call any function that uses the tree (or any function
 * that calls a function that uses the tree), you must set a non-everything
 * duration by calling selectDuration().  The dialog does not initialize the
 * tree if it does not yet need to be shown.
 *
 * @param aWin
 *        The dialog's nsIDOMWindow
 */
function WindowHelper(aWin) {
  this.win = aWin;
}

WindowHelper.prototype = {
  /**
   * "Presses" the dialog's OK button.
   */
  acceptDialog: function () {
    is(this.win.document.documentElement.getButton("accept").disabled, false,
       "Dialog's OK button should not be disabled");
    this.win.document.documentElement.acceptDialog();
  },

  /**
   * "Presses" the dialog's Cancel button.
   */
  cancelDialog: function () {
    this.win.document.documentElement.cancelDialog();
  },

  /**
   * Ensures that the grippy row is in the right place, tree selection is OK,
   * and that the grippy's visible.
   *
   * @param aMsg
   *        Passed to is() when checking grippy location
   * @param aExpectedRow
   *        The row that the grippy should be at
   */
  checkGrippy: function (aMsg, aExpectedRow) {
    is(this.getGrippyRow(), aExpectedRow, aMsg);
    this.checkTreeSelection();
    this.ensureGrippyIsVisible();
  },

  /**
   * (Un)checks a history scope checkbox (browser & download history,
   * form history, etc.).
   *
   * @param aPrefName
   *        The final portion of the checkbox's privacy.cpd.* preference name
   * @param aCheckState
   *        True if the checkbox should be checked, false otherwise
   */
  checkPrefCheckbox: function (aPrefName, aCheckState) {
    var pref = "privacy.cpd." + aPrefName;
    var cb = this.win.document.querySelectorAll(
               "#itemList > [preference='" + pref + "']");
    is(cb.length, 1, "found checkbox for " + pref + " preference");
    if (cb[0].checked != aCheckState)
      cb[0].click();
  },

  /**
   * Ensures that the tree selection is appropriate to the grippy row.  (A
   * single, contiguous selection should exist from the first row all the way
   * to the grippy.)
   */
  checkTreeSelection: function () {
    let grippyRow = this.getGrippyRow();
    let sel = this.getTree().view.selection;
    if (grippyRow === 0) {
      is(sel.getRangeCount(), 0,
         "Grippy row is 0, so no tree selection should exist");
    }
    else {
      is(sel.getRangeCount(), 1,
         "Grippy row > 0, so only one tree selection range should exist");
      let min = {};
      let max = {};
      sel.getRangeAt(0, min, max);
      is(min.value, 0, "Tree selection should start at first row");
      is(max.value, grippyRow - 1,
         "Tree selection should end at row before grippy");
    }
  },

  /**
   * The grippy should always be visible when it's moved directly.  This method
   * ensures that.
   */
  ensureGrippyIsVisible: function () {
    let tbo = this.getTree().treeBoxObject;
    let firstVis = tbo.getFirstVisibleRow();
    let lastVis = tbo.getLastVisibleRow();
    let grippyRow = this.getGrippyRow();
    ok(firstVis <= grippyRow && grippyRow <= lastVis,
       "Grippy row should be visible; this inequality should be true: " +
       firstVis + " <= " + grippyRow + " <= " + lastVis);
  },

  /**
   * @return The dialog's duration dropdown
   */
  getDurationDropdown: function () {
    return this.win.document.getElementById("sanitizeDurationChoice");
  },

  /**
   * @return The grippy row index
   */
  getGrippyRow: function () {
    return this.win.gContiguousSelectionTreeHelper.getGrippyRow();
  },

  /**
   * @return The tree's row count (includes the grippy row)
   */
  getRowCount: function () {
    return this.getTree().view.rowCount;
  },

  /**
   * @return The tree
   */
  getTree: function () {
    return this.win.gContiguousSelectionTreeHelper.tree;
  },

  /**
   * @return True if the "Everything" warning panel is visible (as opposed to
   *         the tree)
   */
  isWarningPanelVisible: function () {
    return this.win.document.getElementById("durationDeck").selectedIndex == 1;
  },

  /**
   * @return True if the tree is visible (as opposed to the warning panel)
   */
  isTreeVisible: function () {
    return this.win.document.getElementById("durationDeck").selectedIndex == 0;
  },

  /**
   * Moves the grippy one row at a time in the direction and magnitude specified.
   * If aDelta < 0, moves the grippy up; if aDelta > 0, moves it down.
   *
   * @param aDelta
   *        The amount and direction to move
   */
  moveGrippyBy: function (aDelta) {
    if (aDelta === 0)
      return;
    let key = aDelta < 0 ? "UP" : "DOWN";
    let abs = Math.abs(aDelta);
    let treechildren = this.getTree().treeBoxObject.treeBody;
    for (let i = 0; i < abs; i++) {
      EventUtils.sendKey(key, treechildren);
    }
  },

  /**
   * Selects a duration in the duration dropdown.
   *
   * @param aDurVal
   *        One of the Sanitizer.TIMESPAN_* values
   */
  selectDuration: function (aDurVal) {
    this.getDurationDropdown().value = aDurVal;
    if (aDurVal === Sanitizer.TIMESPAN_EVERYTHING) {
      is(this.isTreeVisible(), false,
         "Tree should not be visible for TIMESPAN_EVERYTHING");
      is(this.isWarningPanelVisible(), true,
         "Warning panel should be visible for TIMESPAN_EVERYTHING");
    }
    else {
      is(this.isTreeVisible(), true,
         "Tree should be visible for non-TIMESPAN_EVERYTHING");
      is(this.isWarningPanelVisible(), false,
         "Warning panel should not be visible for non-TIMESPAN_EVERYTHING");
    }
  }
};

/**
 * Adds a download to history.
 *
 * @param aMinutesAgo
 *        The download will be downloaded this many minutes ago
 */
function addDownloadWithMinutesAgo(aMinutesAgo) {
  let name = "fakefile-" + aMinutesAgo + "-minutes-ago";
  let data = {
    id:        gDownloadId,
    name:      name,
    source:   "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target:    name,
    startTime: now_uSec - (aMinutesAgo * 60 * 1000000),
    endTime:   now_uSec - ((aMinutesAgo + 1) *60 * 1000000),
    state:     Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume) " +
    "VALUES (:id, :name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume)");
  try {
    for (let prop in data) {
      stmt.params[prop] = data[prop];
    }
    stmt.execute();
  }
  finally {
    stmt.reset();
  }

  is(downloadExists(gDownloadId), true,
     "Sanity check: download " + gDownloadId +
     " should exist after creating it");

  return gDownloadId++;
}

/**
 * Adds a form entry to history.
 *
 * @param aMinutesAgo
 *        The entry will be added this many minutes ago
 */
function addFormEntryWithMinutesAgo(aMinutesAgo) {
  let name = aMinutesAgo + "-minutes-ago";
  formhist.addEntry(name, "dummy");

  // Artifically age the entry to the proper vintage.
  let db = formhist.DBConnection;
  let timestamp = now_uSec - (aMinutesAgo * 60 * 1000000);
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '" + name + "'");

  is(formhist.nameExists(name), true,
     "Sanity check: form entry " + name + " should exist after creating it");
  return name;
}

/**
 * Adds a history visit to history.
 *
 * @param aMinutesAgo
 *        The visit will be visited this many minutes ago
 */
function addHistoryWithMinutesAgo(aMinutesAgo) {
  let pURI = makeURI("http://" + aMinutesAgo + "-minutes-ago.com/");
  bhist.addPageWithDetails(pURI,
                           aMinutesAgo + " minutes ago",
                           now_uSec - (aMinutesAgo * 60 * 1000 * 1000));
  is(bhist.isVisited(pURI), true,
     "Sanity check: history visit " + pURI.spec +
     " should exist after creating it");
  return pURI;
}

/**
 * Removes all history visits, downloads, and form entries.
 */
function blankSlate() {
  bhist.removeAllPages();
  dm.cleanUp();
  formhist.removeAllEntries();
}

/**
 * Checks to see if the download with the specified ID exists.
 *
 * @param  aID
 *         The ID of the download to check
 * @return True if the download exists, false otherwise
 */
function downloadExists(aID)
{
  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE id = :id"
  );
  stmt.params.id = aID;
  let rows = stmt.executeStep();
  stmt.finalize();
  return !!rows;
}

/**
 * Runs the next test in the gAllTests array.  If all tests have been run,
 * finishes the entire suite.
 */
function doNextTest() {
  if (gAllTests.length <= gCurrTest) {
    blankSlate();
    finish();
  }
  else {
    let ct = gCurrTest;
    gCurrTest++;
    gAllTests[ct]();
  }
}

/**
 * Ensures that the specified downloads are either cleared or not.
 *
 * @param aDownloadIDs
 *        Array of download database IDs
 * @param aShouldBeCleared
 *        True if each download should be cleared, false otherwise
 */
function ensureDownloadsClearedState(aDownloadIDs, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aDownloadIDs.forEach(function (id) {
    is(downloadExists(id), !aShouldBeCleared,
       "download " + id + " should " + niceStr + " exist");
  });
}

/**
 * Ensures that the specified form entries are either cleared or not.
 *
 * @param aFormEntries
 *        Array of form entry names
 * @param aShouldBeCleared
 *        True if each form entry should be cleared, false otherwise
 */
function ensureFormEntriesClearedState(aFormEntries, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aFormEntries.forEach(function (entry) {
    is(formhist.nameExists(entry), !aShouldBeCleared,
       "form entry " + entry + " should " + niceStr + " exist");
  });
}

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
function ensureHistoryClearedState(aURIs, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aURIs.forEach(function (aURI) {
    is(bhist.isVisited(aURI), !aShouldBeCleared,
       "history visit " + aURI.spec + " should " + niceStr + " exist");
  });
}

/**
 * Opens the sanitize dialog and runs a callback once it's finished loading.
 * 
 * @param aOnloadCallback
 *        A function that will be called once the dialog has loaded
 */
function openWindow(aOnloadCallback) {
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened")
      return;

    Services.ww.unregisterNotification(windowObserver);
    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function onload(event) {
      win.removeEventListener("load", onload, false);
      executeSoon(function () {
        // Some exceptions that reach here don't reach the test harness, but
        // ok()/is() do...
        try {
          aOnloadCallback(win);
          doNextTest();
        }
        catch (exc) {
          win.close();
          ok(false, "Unexpected exception: " + exc + "\n" + exc.stack);
          finish();
        }
      });
    }, false);
  }
  Services.ww.registerNotification(windowObserver);
  Services.ww.openWindow(null,
                         "chrome://browser/content/sanitize.xul",
                         "Sanitize",
                         "chrome,titlebar,dialog,centerscreen,modal",
                         null);
}

///////////////////////////////////////////////////////////////////////////////

function test() {
  blankSlate();
  waitForExplicitFinish();
  // Kick off all the tests in the gAllTests array.
  doNextTest();
}
