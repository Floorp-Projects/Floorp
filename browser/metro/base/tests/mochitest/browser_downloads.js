/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Provides infrastructure for automated download components tests.
 * (adapted from browser/component/downloads test's head.js)
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

const nsIDM = Ci.nsIDownloadManager;

////////////////////////////////////////////////////////////////////////////////
// Test Helpers

var { spawn } = Task;

function equalStrings(){
  let ref = ""+arguments[0];
  for (let i=1; i<arguments.length; i++){
    if (ref !== ""+arguments[i]) {
      info("equalStrings failure: " + ref + " != " + arguments[i]);
      return false
    }
  }
  return true;
}

function equalNumbers(){
  let ref = Number(arguments[0]);
  for (let i=1; i<arguments.length; i++){
    if (ref !== Number(arguments[i])) return false;
    if (ref !== Number(arguments[i])) {
      info("equalNumbers failure: " + ref + " != " + Number(arguments[i]));
      return false
    }
  }
  return true;
}

function getPromisedDbResult(aStatement) {
  let dbConnection = MetroDownloadsView.manager.DBConnection;
  let statement = ("string" == typeof aStatement) ?
        dbConnection.createAsyncStatement(
          aStatement
        ) : aStatement;

  let deferred = Promise.defer(),
      resultRows = [],
      err = null;
  try {
    statement.executeAsync({
      handleResult: function(aResultSet) {
        let row;
        if(!aResultSet) {
          return;
        }
        while ((row = aResultSet.getNextRow())){
          resultRows.push(row);
        }
      },
      handleError: function(aError) {
        Cu.reportError(aError);
        err = aError;
      },
      handleCompletion: function(){
        if (err) {
          deferred.reject(err);
        } else {
          deferred.resolve(resultRows);
        }
     }
    });
  } finally {
    statement.finalize();
  }
  return deferred.promise;
}

let gTestTargetFile = FileUtils.getFile("TmpD", ["dm-ui-test.file"]);
gTestTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
registerCleanupFunction(function () {
  gTestTargetFile.remove(false);
  PanelUI.hide();
});

/**
 * This object contains a property for each column in the downloads table.
 */
let gDownloadRowTemplate = {
  name: "test-download.txt",
  source: "http://www.example.com/test-download.txt",
  target: NetUtil.newURI(gTestTargetFile).spec,
  startTime: 1180493839859230,
  endTime: 1180493839859234,
  state: nsIDM.DOWNLOAD_FINISHED,
  currBytes: 0,
  maxBytes: -1,
  preferredAction: 0,
  autoResume: 0
};

////////////////////////////////////////////////////////////////////////////////
// Test Infrastructure

function test() {
  runTests();
}

/////////////////////////////////////
// shared test setup
function resetDownloads(){
  // clear out existing and any pending downloads in the db
  // returns a promise

  let promisedResult = getPromisedDbResult(
    "DELETE FROM moz_downloads"
  );
  return promisedResult.then(function(aResult){
    // // Reset any prefs that might have been changed.
    // Services.prefs.clearUserPref("browser.download.panel.shown");

    // Ensure that data is unloaded.
    let dlMgr = MetroDownloadsView.manager;
    let dlsToRemove = [];
    // Clear all completed/cancelled downloads
    dlMgr.cleanUp();
    dlMgr.cleanUpPrivate();

    // Queue up all active ones as well
    for (let dlsEnum of [dlMgr.activeDownloads, dlMgr.activePrivateDownloads]) {
      while (dlsEnum.hasMoreElements()) {
        dlsToRemove.push(dlsEnum.next());
      }
    }
    // Remove any queued up active downloads
    dlsToRemove.forEach(function (dl) {
      dl.remove();
    });
  });
}

function addDownloadRow(aDataRow) {
  let deferredInsert = Promise.defer();
  let dataRow = aDataRow;

  let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  let columnNames = Object.keys(gDownloadRowTemplate).join(", ");
  let parameterNames = Object.keys(gDownloadRowTemplate)
                             .map(function(n) ":" + n)
                             .join(", ");

  let statement = db.createAsyncStatement(
                  "INSERT INTO moz_downloads (" + columnNames +
                  ", guid) VALUES(" + parameterNames + ", GENERATE_GUID())");

  // Populate insert parameters from the provided data.
  for (let columnName in gDownloadRowTemplate) {
    if (!(columnName in dataRow)) {
      // Update the provided row object with data from the global template,
      // for columns whose value is not provided explicitly.
      dataRow[columnName] = gDownloadRowTemplate[columnName];
    }
    statement.params[columnName] = dataRow[columnName];
  }

  // Run the statement asynchronously and wait.
  let promisedDownloads = getPromisedDbResult(
    statement
  );
  yield promisedDownloads.then(function(){
      let newItemId = db.lastInsertRowID;
      let download = dm.getDownload(newItemId);
      deferredInsert.resolve(download);
  });
}

function gen_addDownloadRows(aDataRows){
  if (!aDataRows.length) {
    yield null;
  }

  try {
    // Add each of the provided downloads in reverse.
    for (let i = aDataRows.length - 1; i >= 0; i--) {
      let dataRow = aDataRows[i];
      let download = yield addDownloadRow(dataRow);

      // At each iteration, ensure that the start and end time in the global
      // template is distinct, as these column are used to sort each download
      // in its category.
      gDownloadRowTemplate.startTime++;
      gDownloadRowTemplate.endTime++;
    }
  } finally {
    info("gen_addDownloadRows, finally");
  }
}

/////////////////////////////////////
// Test implementations

gTests.push({
  desc: "zero downloads",
  run: function () {
    yield resetDownloads();
    todo(false, "Test there are no visible notifications with an empty db.");
  }
});

/**
 * Make sure the downloads panel can display items in the right order and
 * contains the expected data.
 */
gTests.push({
  desc: "Show downloads",
  run: function(){
    // Display one of each download state.
    let DownloadData = [
      { endTime: 1180493839859239, state: nsIDM.DOWNLOAD_NOTSTARTED },
      { endTime: 1180493839859238, state: nsIDM.DOWNLOAD_DOWNLOADING },
      { endTime: 1180493839859237, state: nsIDM.DOWNLOAD_PAUSED },
      { endTime: 1180493839859236, state: nsIDM.DOWNLOAD_SCANNING },
      { endTime: 1180493839859235, state: nsIDM.DOWNLOAD_QUEUED },
      { endTime: 1180493839859234, state: nsIDM.DOWNLOAD_FINISHED },
      { endTime: 1180493839859233, state: nsIDM.DOWNLOAD_FAILED },
      { endTime: 1180493839859232, state: nsIDM.DOWNLOAD_CANCELED },
      { endTime: 1180493839859231, state: nsIDM.DOWNLOAD_BLOCKED_PARENTAL },
      { endTime: 1180493839859230, state: nsIDM.DOWNLOAD_DIRTY },
      { endTime: 1180493839859229, state: nsIDM.DOWNLOAD_BLOCKED_POLICY }
    ];

    yield resetDownloads();

    try {
      // Populate the downloads database with the data required by this test.
      // we're going to add stuff to the downloads db.
      yield spawn( gen_addDownloadRows( DownloadData ) );

      todo( false, "Check that MetroDownloadsView._progressNotificationInfo and MetroDownloadsView._downloadCount \
        have the correct length (DownloadData.length) \
        May also test that the correct notifications show up for various states.");

      todo(false, "Iterate through download objects in MetroDownloadsView._progressNotificationInfo \
        and confirm that the downloads they refer to are the same as those in \
        DownloadData.");
    } catch(e) {
      info("Show downloads, some error: " + e);
    }
    finally {
      // Clean up when the test finishes.
      yield resetDownloads();
    }
  }
});

/**
 * Make sure the downloads can be removed with the expected result on the notifications
 */
gTests.push({
  desc: "Remove downloads",
  run: function(){
    // Push a few items into the downloads db.
    let DownloadData = [
      { endTime: 1180493839859239, state: nsIDM.DOWNLOAD_FINISHED },
      { endTime: 1180493839859238, state: nsIDM.DOWNLOAD_FINISHED },
      { endTime: 1180493839859237, state: nsIDM.DOWNLOAD_FINISHED }
    ];

    yield resetDownloads();

    try {
      // Populate the downloads database with the data required by this test.
      yield spawn( gen_addDownloadRows( DownloadData ) );

      let downloadRows = null,
          promisedDownloads;
      // get all the downloads from the db
      promisedDownloads = getPromisedDbResult(
        "SELECT guid "
      + "FROM moz_downloads "
      + "ORDER BY startTime DESC"
      ).then(function(aRows){
        downloadRows = aRows;
      }, function(aError){
        throw aError;
      });
      yield promisedDownloads;

      is(downloadRows.length, 3, "Correct number of downloads in the db before removal");

      todo(false, "Get some download from MetroDownloadsView._progressNotificationInfo, \
        confirm that its file exists, then remove it.");

      // remove is async(?), wait a bit
      yield waitForMs(0);

      // get all the downloads from the db
      downloadRows = null;
      promisedDownloads = getPromisedDbResult(
        "SELECT guid "
      + "FROM moz_downloads "
      + "ORDER BY startTime DESC"
      ).then(function(aRows){
        downloadRows = aRows;
      }, function(aError){
        throw aError;
      });
      yield promisedDownloads;

      todo(false, "confirm that the removed download is no longer in the database \
        and its file no longer exists.");

    } catch(e) {
      info("Remove downloads, some error: " + e);
    }
    finally {
      // Clean up when the test finishes.
      yield resetDownloads();
    }
  }
});

/**
 * Make sure the cancelled/aborted downloads are handled correctly.
 */
gTests.push({
  desc: "Cancel/Abort Downloads",
  run: function(){
    todo(false, "Ensure that a cancelled/aborted download is in the correct state \
      including correct values for state variables (e.g. _downloadCount, _downloadsInProgress) \
      and the existence of the downloaded file.");
  }
});

/**
 * Make sure download notifications are moved when we close tabs.
 */
gTests.push({
  desc: "Download notifications in closed tabs",
  setUp: function() {
    // put up a couple notifications on the initial tab
    let notificationBox = Browser.getNotificationBox();
    notificationBox.appendNotification("not important", "low-priority-thing", "", notificationBox.PRIORITY_INFO_LOW, []);
    notificationBox.appendNotification("so important", "high-priority-thing", "", notificationBox.PRIORITY_CRITICAL_HIGH, []);

    // open a new tab where we'll conduct the test
    yield addTab("about:mozilla");
  },
  run: function(){
    let notificationBox = Browser.getNotificationBox();
    let notn = MetroDownloadsView.showNotification("download-progress", "test message", [],
           notificationBox.PRIORITY_WARNING_LOW);
    Browser.closeTab(Browser.selectedTab);

    yield waitForEvent(Elements.tabList, "TabRemove");

    // expected behavior when a tab is closed while a download notification is showing:
    // * the notification remains visible as long as a next tab/browser exists
    // * normal rules about priority apply
    // * notifications - including any pre-existing ones - display in expected order
    let nextBox = Browser.getNotificationBox();
    let currentNotification;

    ok(nextBox.getNotificationWithValue("download-progress"), "notification was moved to next tab");

    currentNotification = nextBox.currentNotification;
    is(currentNotification.value, "high-priority-thing", "high priority notification is current");
    currentNotification.close();

    currentNotification = nextBox.currentNotification;
    is(currentNotification.value, "download-progress", "download notification is next");
    currentNotification.close();

    currentNotification = nextBox.currentNotification;
    is(currentNotification.value, "low-priority-thing", "low priority notification is next");
    currentNotification.close();
  }
});