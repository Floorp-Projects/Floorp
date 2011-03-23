// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a test harness for running javascript tests in the browser.
// The only identifier exposed by this harness is WebGLTestHarnessModule.
//
// To use it make an HTML page with an iframe. Then call the harness like this
//
//    function reportResults(type, msg, success) {
//      ...
//      return true;
//    }
//
//    var fileListURL = '00_test_list.txt';
//    var testHarness = new WebGLTestHarnessModule.TestHarness(
//        iframe,
//        fileListURL,
//        reportResults);
//
// The harness will load the fileListURL and parse it for the URLs, one URL
// per line. URLs should be on the same domain and at the same folder level
// or below the main html file.  If any URL ends in .txt it will be parsed
// as well so you can nest .txt files. URLs inside a .txt file should be
// relative to that text file.
//
// During startup, for each page found the reportFunction will be called with
// WebGLTestHarnessModule.TestHarness.reportType.ADD_PAGE and msg will be
// the URL of the test.
//
// Each test is required to call testHarness.reportResults. This is most easily
// accomplished by storing that value on the main window with
//
//     window.webglTestHarness = testHarness
//
// and then adding these to functions to your tests.
//
//     function reportTestResultsToHarness(success, msg) {
//       if (window.parent.webglTestHarness) {
//         window.parent.webglTestHarness.reportResults(success, msg);
//       }
//     }
//
//     function notifyFinishedToHarness() {
//       if (window.parent.webglTestHarness) {
//         window.parent.webglTestHarness.notifyFinished();
//       }
//     }
//
// This way your tests will still run without the harness and you can use
// any testing framework you want.
//
// Each test should call reportTestResultsToHarness with true for success if it
// succeeded and false if it fail followed and any message it wants to
// associate with the test. If your testing framework supports checking for
// timeout you can call it with success equal to undefined in that case.
//
// To run the tests, call testHarness.runTests();
//
// For each test run, before the page is loaded the reportFunction will be
// called with WebGLTestHarnessModule.TestHarness.reportType.START_PAGE and msg
// will be the URL of the test. You may return false if you want the test to be
// skipped.
//
// For each test completed the reportFunction will be called with
// with WebGLTestHarnessModule.TestHarness.reportType.TEST_RESULT,
// success = true on success, false on failure, undefined on timeout
// and msg is any message the test choose to pass on.
//
// When all the tests on the page have finished your page must call
// notifyFinishedToHarness.  If notifyFinishedToHarness is not called
// the harness will assume the test timed out.
//
// When all the tests on a page have finished OR the page as timed out the
// reportFunction will be called with
// WebGLTestHarnessModule.TestHarness.reportType.FINISH_PAGE
// where success = true if the page has completed or undefined if the page timed
// out.
//
// Finally, when all the tests have completed the reportFunction will be called
// with WebGLTestHarnessModule.TestHarness.reportType.FINISHED_ALL_TESTS.
//

WebGLTestHarnessModule = function() {

/**
 * Wrapped logging function.
 */
var log = function(msg) {
  if (window.console && window.console.log) {
    window.console.log(msg);
  }
};

/**
 * Loads text from an external file. This function is synchronous.
 * @param {string} url The url of the external file.
 * @return {string} the loaded text if the request is synchronous.
 */
var loadTextFileSynchronous = function(url) {
  var error = 'loadTextFileSynchronous failed to load url "' + url + '"';
  var request;
  if (window.XMLHttpRequest) {
    request = new XMLHttpRequest();
    if (request.overrideMimeType) {
      request.overrideMimeType('text/plain');
    }
  } else {
    throw 'XMLHttpRequest is disabled';
  }
  request.open('GET', url, false);
  request.send(null);
  if (request.readyState != 4) {
    throw error;
  }
  return request.responseText;
};

var getFileList = function(url) {
  var files = [];
  if (url.substr(url.length - 4) == '.txt') {
    var lines = loadTextFileSynchronous(url).split('\n');
    var prefix = '';
    var lastSlash = url.lastIndexOf('/');
    if (lastSlash >= 0) {
      prefix = url.substr(0, lastSlash + 1);
    }
    for (var ii = 0; ii < lines.length; ++ii) {
      var str = lines[ii].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
      if (str.length > 4 &&
          str[0] != '#' &&
          str[0] != ";" &&
          str.substr(0, 2) != "//") {
        new_url = prefix + str;
        files = files.concat(getFileList(new_url));
      }
    }
  } else {
    files.push(url);
  }
  return files;
}

var TestFile = function(url) {
  this.url = url;
};

var TestHarness = function(iframe, filelistUrl, reportFunc) {
  this.window = window;
  this.iframe = iframe;
  this.reportFunc = reportFunc;
  var files = getFileList(filelistUrl);
  this.files = [];
  for (var ii = 0; ii < files.length; ++ii) {
    this.files.push(new TestFile(files[ii]));
    this.reportFunc(TestHarness.reportType.ADD_PAGE, files[ii], undefined);
  }
  this.nextFileIndex = files.length;
  this.timeoutDelay = 20000;
};

TestHarness.reportType = {
  ADD_PAGE: 1,
  START_PAGE: 2,
  TEST_RESULT: 3,
  FINISH_PAGE: 4,
  FINISHED_ALL_TESTS: 5
};

TestHarness.prototype.runTests = function(files) {
  this.nextFileIndex = 0;
  this.startNextFile();
};

TestHarness.prototype.setTimeout = function() {
  var that = this;
  this.timeoutId = this.window.setTimeout(function() {
      that.timeout();
    }, this.timeoutDelay);
};

TestHarness.prototype.clearTimeout = function() {
  this.window.clearTimeout(this.timeoutId);
};

TestHarness.prototype.startNextFile = function() {
  if (this.nextFileIndex >= this.files.length) {
    log("done");
    this.reportFunc(TestHarness.reportType.FINISHED_ALL_TESTS,
                    '', undefined);
  } else {
    this.currentFile = this.files[this.nextFileIndex++];
    log("loading: " + this.currentFile.url);
    if (this.reportFunc(TestHarness.reportType.START_PAGE,
                        this.currentFile.url, undefined)) {
      this.iframe.src = this.currentFile.url;
      this.setTimeout();
    } else {
      this.reportResults(false, "skipped");
      this.notifyFinished();
    }
  }
};

TestHarness.prototype.reportResults = function (success, msg) {
  this.clearTimeout();
  log(success ? "PASS" : "FAIL", msg);
  this.reportFunc(TestHarness.reportType.TEST_RESULT, msg, success);
  // For each result we get, reset the timeout
  this.setTimeout();
};

TestHarness.prototype.notifyFinished = function () {
  this.clearTimeout();
  var url = this.currentFile ? this.currentFile.url : 'unknown';
  log(url + ": finished");
  this.reportFunc(TestHarness.reportType.FINISH_PAGE, url, true);
  this.startNextFile();
};

TestHarness.prototype.timeout = function() {
  this.clearTimeout();
  var url = this.currentFile ? this.currentFile.url : 'unknown';
  log(url + ": timeout");
  this.reportFunc(TestHarness.reportType.FINISH_PAGE, url, undefined);
  this.startNextFile();
};

TestHarness.prototype.setTimeoutDelay = function(x) {
  this.timeoutDelay = x;
};

return {
    'TestHarness': TestHarness
  };

}();



