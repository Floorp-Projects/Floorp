// A common module to run tests on the AccessFu module

/**
  * A global variable holding an array of test functions.
  */
var gTestFuncs = [];
/**
  * A global Iterator for the array of test functions.
  */
var gIterator;

Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import("resource://gre/modules/accessibility/Utils.jsm");
Components.utils.import("resource://gre/modules/accessibility/EventManager.jsm");

var AccessFuTest = {

  addFunc: function AccessFuTest_addFunc(aFunc) {
    if (aFunc) {
      gTestFuncs.push(aFunc);
    }
  },

  _waitForExplicitFinish: false,

  waitForExplicitFinish: function AccessFuTest_waitForExplicitFinish() {
    this._waitForExplicitFinish = true;
  },

  finish: function AccessFuTest_finish() {
    // Disable the console service logging.
    Logger.test = false;
    AccessFu.doneCallback = function doneCallback() {
      // This is being called once AccessFu has been shut down.
      // Detach AccessFu from everything it attached itself to.
      AccessFu.detach();
      // and finish the test run.
      SimpleTest.finish();
    };
    // Tear down accessibility and make AccessFu stop.
    SpecialPowers.setIntPref("accessibility.accessfu.activate", 0);
  },

  nextTest: function AccessFuTest_nextTest() {
    var testFunc;
    try {
      // Get the next test function from the iterator. If none left,
      // StopIteration exception is thrown.
      testFunc = gIterator.next()[1];
    } catch (ex) {
      // StopIteration exception.
      this.finish();
      return;
    }
    testFunc();
  },

  runTests: function AccessFuTest_runTests() {
    if (gTestFuncs.length === 0) {
      ok(false, "No tests specified!");
      simpleTest.finish();
      return;
    }

    // Create an Iterator for gTestFuncs array.
    gIterator = Iterator(gTestFuncs);

    // Start AccessFu and put it in stand-by.
    Components.utils.import("resource://gre/modules/accessibility/AccessFu.jsm");

    AccessFu.attach(getMainChromeWindow(window));

    AccessFu.readyCallback = function readyCallback() {
      // Enable logging to the console service.
      Logger.test = true;
      // This is being called once accessibility has been turned on.

      if (AccessFuTest._waitForExplicitFinish) {
        // Run all test functions asynchronously.
        AccessFuTest.nextTest();
      } else {
        // Run all test functions synchronously.
        [testFunc() for (testFunc of gTestFuncs)];
        AccessFuTest.finish();
      }
    };

    // Invoke the whole thing.
    SpecialPowers.setIntPref("accessibility.accessfu.activate", 1);
  }
};
