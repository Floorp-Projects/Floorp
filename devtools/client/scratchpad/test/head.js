/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const {console} = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
const {require} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const promise = require("promise");


var gScratchpadWindow; // Reference to the Scratchpad chrome window object

DevToolsUtils.testing = true;
SimpleTest.registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

/**
 * Open a Scratchpad window.
 *
 * @param function aReadyCallback
 *        Optional. The function you want invoked when the Scratchpad instance
 *        is ready.
 * @param object aOptions
 *        Optional. Options for opening the scratchpad:
 *        - window
 *          Provide this if there's already a Scratchpad window you want to wait
 *          loading for.
 *        - state
 *          Scratchpad state object. This is used when Scratchpad is open.
 *        - noFocus
 *          Boolean that tells you do not want the opened window to receive
 *          focus.
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad. Note that the
 *         gScratchpadWindow global is also updated to reference the new window
 *         object.
 */
function openScratchpad(aReadyCallback, aOptions = {})
{
  let win = aOptions.window ||
            Scratchpad.ScratchpadManager.openScratchpad(aOptions.state);
  if (!win) {
    return;
  }

  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    win.Scratchpad.addObserver({
      onReady: function(aScratchpad) {
        aScratchpad.removeObserver(this);

        if (aOptions.noFocus) {
          aReadyCallback(win, aScratchpad);
        } else {
          waitForFocus(aReadyCallback.bind(null, win, aScratchpad), win);
        }
      }
    });
  };

  if (aReadyCallback) {
    win.addEventListener("load", onLoad, false);
  }

  gScratchpadWindow = win;
  return gScratchpadWindow;
}

/**
 * Open a new tab and then open a scratchpad.
 * @param object aOptions
 *        Optional. Options for opening the tab and scratchpad. In addition
 *        to the options supported by openScratchpad, the following options
 *        are supported:
 *        - tabContent
 *          A string providing the html content of the tab.
 * @return Promise
 */
function openTabAndScratchpad(aOptions = {})
{
  waitForExplicitFinish();
  return new promise(resolve => {
    gBrowser.selectedTab = gBrowser.addTab();
    let {selectedBrowser} = gBrowser;
    selectedBrowser.addEventListener("load", function onLoad() {
      selectedBrowser.removeEventListener("load", onLoad, true);
      openScratchpad((win, sp) => resolve([win, sp]), aOptions);
    }, true);
    content.location = "data:text/html;charset=utf8," + (aOptions.tabContent || "");
  });
}

/**
 * Create a temporary file, write to it and call a callback
 * when done.
 *
 * @param string aName
 *        Name of your temporary file.
 * @param string aContent
 *        Temporary file's contents.
 * @param function aCallback
 *        Optional callback to be called when we're done writing
 *        to the file. It will receive two parameters: status code
 *        and a file object.
 */
function createTempFile(aName, aContent, aCallback=function(){})
{
  // Create a temporary file.
  let file = FileUtils.getFile("TmpD", [aName]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  // Write the temporary file.
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(file.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            parseInt("644", 8), fout.DEFER_OPEN);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let fileContentStream = converter.convertToInputStream(aContent);

  NetUtil.asyncCopy(fileContentStream, fout, function (aStatus) {
    aCallback(aStatus, file);
  });
}

/**
 * Run a set of asychronous tests sequentially defined by input and output.
 *
 * @param Scratchpad aScratchpad
 *        The scratchpad to use in running the tests.
 * @param array aTests
 *        An array of test objects, each with the following properties:
 *        - method
 *          Scratchpad method to use, one of "run", "display", or "inspect".
 *        - code
 *          Code to run in the scratchpad.
 *        - result
 *          Expected code that will be in the scratchpad upon completion.
 *        - label
 *          The tests label which will be logged in the test runner output.
 * @return Promise
 *         The promise that will be resolved when all tests are finished.
 */
function runAsyncTests(aScratchpad, aTests)
{
  let deferred = promise.defer();

  (function runTest() {
    if (aTests.length) {
      let test = aTests.shift();
      aScratchpad.setText(test.code);
      aScratchpad[test.method]().then(function success() {
        is(aScratchpad.getText(), test.result, test.label);
        runTest();
      }, function failure(error) {
        ok(false, error.stack + " " + test.label);
        runTest();
      });
    } else {
      deferred.resolve();
    }
  })();

  return deferred.promise;
}

/**
 * Run a set of asychronous tests sequentially with callbacks to prepare each
 * test and to be called when the test result is ready.
 *
 * @param Scratchpad aScratchpad
 *        The scratchpad to use in running the tests.
 * @param array aTests
 *        An array of test objects, each with the following properties:
 *        - method
 *          Scratchpad method to use, one of "run", "display", or "inspect".
 *        - prepare
 *          The callback to run just prior to executing the scratchpad method.
 *        - then
 *          The callback to run when the scratchpad execution promise resolves.
 * @return Promise
 *         The promise that will be resolved when all tests are finished.
 */
function runAsyncCallbackTests(aScratchpad, aTests)
{
  let deferred = promise.defer();

  (function runTest() {
    if (aTests.length) {
      let test = aTests.shift();
      test.prepare();
      aScratchpad[test.method]().then(test.then.bind(test)).then(runTest);
    } else {
      deferred.resolve();
    }
  })();

  return deferred.promise;
}

function cleanup()
{
  if (gScratchpadWindow) {
    gScratchpadWindow.close();
    gScratchpadWindow = null;
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

registerCleanupFunction(cleanup);
