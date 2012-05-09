/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

const nsIDM = Ci.nsIDownloadManager;

let gTestTargetFile = FileUtils.getFile("TmpD", ["dm-ui-test.file"]);
gTestTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
registerCleanupFunction(function () {
  gTestTargetFile.remove(false);
});

/**
 * This objects contains a property for each column in the downloads table.
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
//// Infrastructure

// All test are run through the test runner.
function test()
{
  testRunner.runTest(this.gen_test);
}

/**
 * Runs a browser-chrome test defined through a generator function.
 *
 * This object is a singleton, initialized automatically when this script is
 * included.  Every browser-chrome test file includes a new copy of this object.
 */
var testRunner = {
  _testIterator: null,
  _lastEventResult: undefined,
  _testRunning: false,
  _eventRaised: false,

  // --- Main test runner ---

  /**
   * Runs the test described by the provided generator function asynchronously.
   *
   * Calling yield in the generator will cause it to wait until continueTest is
   * called.  The parameter provided to continueTest will be the return value of
   * the yield operator.
   *
   * @param aGenerator
   *        Test generator function.  The function will be called with no
   *        arguments to retrieve its iterator.
   */
  runTest: function TR_runTest(aGenerator) {
    waitForExplicitFinish();
    testRunner._testIterator = aGenerator();
    testRunner.continueTest();
  },

  /**
   * Continues the currently running test.
   *
   * @param aEventResult
   *        This will be the return value of the yield operator in the test.
   */
  continueTest: function TR_continueTest(aEventResult) {
    // Store the last event result, or set it to undefined.
    testRunner._lastEventResult = aEventResult;

    // Never reenter the main loop, but notify that the event has been raised.
    if (testRunner._testRunning) {
      testRunner._eventRaised = true;
      return;
    }

    // Enter the main iteration loop.
    testRunner._testRunning = true;
    try {
      do {
        // Call the iterator, but don't leave the loop if the expected event is
        // raised during the execution of the generator.
        testRunner._eventRaised = false;
        testRunner._testIterator.send(testRunner._lastEventResult);
      } while (testRunner._eventRaised);
    }
    catch (e) {
      // This block catches exceptions raised by the generator, including the
      // normal StopIteration exception.  Unexpected exceptions are reported as
      // test failures.
      if (!(e instanceof StopIteration))
        ok(false, e);
      // In any case, stop the tests in this file.
      finish();
    }

    // Wait for the next event or finish.
    testRunner._testRunning = false;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Asynchronous generator-based support subroutines

//
// The following functions are all generators that can be used inside the main
// test generator to perform specific tasks asynchronously.  To invoke these
// subroutines correctly, an iteration syntax should be used:
//
//   for (let yy in gen_example("Parameter")) yield;
//

function gen_resetState()
{
  let statement = Services.downloads.DBConnection.createAsyncStatement(
                  "DELETE FROM moz_downloads");
  try {
    statement.executeAsync({
      handleResult: function(aResultSet) { },
      handleError: function(aError)
      {
        Cu.reportError(aError);
      },
      handleCompletion: function(aReason)
      {
        testRunner.continueTest();
      }
    });
    yield;
  } finally {
    statement.finalize();
  }

  // Ensure that the panel is closed and data is unloaded.
  DownloadsCommon.data.clear();
  DownloadsCommon.data._loadState = DownloadsCommon.data.kLoadNone;

  // Wait for focus on the main window.
  waitForFocus(testRunner.continueTest);
  yield;
}

function gen_addDownloadRows(aDataRows)
{
  let columnNames = Object.keys(gDownloadRowTemplate).join(", ");
  let parameterNames = Object.keys(gDownloadRowTemplate)
                             .map(function(n) ":" + n)
                             .join(", ");
  let statement = Services.downloads.DBConnection.createAsyncStatement(
                  "INSERT INTO moz_downloads (" + columnNames +
                                    ") VALUES(" + parameterNames + ")");
  try {
    // Execute the statement for each of the provided downloads in reverse.
    for (let i = aDataRows.length - 1; i >= 0; i--) {
      let dataRow = aDataRows[i];

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
      statement.executeAsync({
        handleResult: function(aResultSet) { },
        handleError: function(aError)
        {
          Cu.reportError(aError);
        },
        handleCompletion: function(aReason)
        {
          testRunner.continueTest();
        }
      });
      yield;

      // At each iteration, ensure that the end time in the global template is
      // distinct, as this column is used to sort each download in its category.
      gDownloadRowTemplate.endTime++;
    }
  } finally {
    statement.finalize();
  }
}

function gen_openPanel(aData)
{
  // Hook to wait until the test data has been loaded.
  let originalOnViewLoadCompleted = DownloadsPanel.onViewLoadCompleted;
  DownloadsPanel.onViewLoadCompleted = function () {
    DownloadsPanel.onViewLoadCompleted = originalOnViewLoadCompleted;
    originalOnViewLoadCompleted.apply(this);
    testRunner.continueTest();
  };

  // Start loading all the downloads from the database asynchronously.
  DownloadsCommon.data.ensurePersistentDataLoaded(false);

  // Wait for focus on the main window.
  waitForFocus(testRunner.continueTest);
  yield;

  // Open the downloads panel, waiting until loading is completed.
  DownloadsPanel.showPanel();
  yield;
}
