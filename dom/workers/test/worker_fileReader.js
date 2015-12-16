var testRanCounter = 0;
var expectedTestCount = 0;
var testSetupFinished = false;

function ok(a, msg) {
  postMessage({type: 'check', status: !!a, msg: msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({type: 'finish'});
}

function convertToUTF16(s) {
  res = "";
  for (var i = 0; i < s.length; ++i) {
    c = s.charCodeAt(i);
    res += String.fromCharCode(c & 255, c >>> 8);
  }
  return res;
}

function convertToUTF8(s) {
  return unescape(encodeURIComponent(s));
}

function convertToDataURL(s) {
  return "data:application/octet-stream;base64," + btoa(s);
}

onmessage = function(message) {
  is(FileReader.EMPTY, 0, "correct EMPTY value");
  is(FileReader.LOADING, 1, "correct LOADING value");
  is(FileReader.DONE, 2, "correct DONE value");

  // List of blobs.
  var asciiFile = message.data.blobs.shift();
  var binaryFile = message.data.blobs.shift();
  var nonExistingFile = message.data.blobs.shift();
  var utf8TextFile = message.data.blobs.shift();
  var utf16TextFile = message.data.blobs.shift();
  var emptyFile = message.data.blobs.shift();
  var dataUrlFile0 = message.data.blobs.shift();
  var dataUrlFile1 = message.data.blobs.shift();
  var dataUrlFile2 = message.data.blobs.shift();

  // List of buffers for testing.
  var testTextData = message.data.testTextData;
  var testASCIIData = message.data.testASCIIData;
  var testBinaryData = message.data.testBinaryData;
  var dataurldata0 = message.data.dataurldata0;
  var dataurldata1 = message.data.dataurldata1;
  var dataurldata2 = message.data.dataurldata2;

  // Test that plain reading works and fires events as expected, both
  // for text and binary reading

  var onloadHasRunText = false;
  var onloadStartHasRunText = false;
  r = new FileReader();
  is(r.readyState, FileReader.EMPTY, "correct initial text readyState");
  r.onload = getLoadHandler(testASCIIData, testASCIIData.length, "plain reading");
  r.addEventListener("load", function() { onloadHasRunText = true }, false);
  r.addEventListener("loadstart", function() { onloadStartHasRunText = true }, false);
  r.readAsText(asciiFile);
  is(r.readyState, FileReader.LOADING, "correct loading text readyState");
  is(onloadHasRunText, false, "text loading must be async");
  is(onloadStartHasRunText, true, "text loadstart should fire sync");
  expectedTestCount++;

  var onloadHasRunBinary = false;
  var onloadStartHasRunBinary = false;
  r = new FileReader();
  is(r.readyState, FileReader.EMPTY, "correct initial binary readyState");
  r.addEventListener("load", function() { onloadHasRunBinary = true }, false);
  r.addEventListener("loadstart", function() { onloadStartHasRunBinary = true }, false);
  r.readAsBinaryString(binaryFile);
  r.onload = getLoadHandler(testBinaryData, testBinaryData.length, "binary reading");
  is(r.readyState, FileReader.LOADING, "correct loading binary readyState");
  is(onloadHasRunBinary, false, "binary loading must be async");
  is(onloadStartHasRunBinary, true, "binary loadstart should fire sync");
  expectedTestCount++;

  var onloadHasRunArrayBuffer = false;
  var onloadStartHasRunArrayBuffer = false;
  r = new FileReader();
  is(r.readyState, FileReader.EMPTY, "correct initial arrayBuffer readyState");
  r.addEventListener("load", function() { onloadHasRunArrayBuffer = true }, false);
  r.addEventListener("loadstart", function() { onloadStartHasRunArrayBuffer = true }, false);
  r.readAsArrayBuffer(binaryFile);
  r.onload = getLoadHandlerForArrayBuffer(testBinaryData, testBinaryData.length, "array buffer reading");
  is(r.readyState, FileReader.LOADING, "correct loading arrayBuffer readyState");
  is(onloadHasRunArrayBuffer, false, "arrayBuffer loading must be async");
  is(onloadStartHasRunArrayBuffer, true, "arrayBuffer loadstart should fire sync");
  expectedTestCount++;

  // Test a variety of encodings, and make sure they work properly
  r = new FileReader();
  r.onload = getLoadHandler(testASCIIData, testASCIIData.length, "no encoding reading");
  r.readAsText(asciiFile, "");
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler(testASCIIData, testASCIIData.length, "iso8859 reading");
  r.readAsText(asciiFile, "iso-8859-1");
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler(testTextData,
                            convertToUTF8(testTextData).length,
                            "utf8 reading");
  r.readAsText(utf8TextFile, "utf8");
  expectedTestCount++;

  r = new FileReader();
  r.readAsText(utf16TextFile, "utf-16");
  r.onload = getLoadHandler(testTextData,
                            convertToUTF16(testTextData).length,
                            "utf16 reading");
  expectedTestCount++;

  // Test get result without reading
  r = new FileReader();
  is(r.readyState, FileReader.EMPTY,
     "readyState in test reader get result without reading");
  is(r.error, null,
     "no error in test reader get result without reading");
  is(r.result, null,
     "result in test reader get result without reading");

  // Test loading an empty file works (and doesn't crash!)
  r = new FileReader();
  r.onload = getLoadHandler("", 0, "empty no encoding reading");
  r.readAsText(emptyFile, "");
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler("", 0, "empty utf8 reading");
  r.readAsText(emptyFile, "utf8");
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler("", 0, "empty utf16 reading");
  r.readAsText(emptyFile, "utf-16");
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler("", 0, "empty binary string reading");
  r.readAsBinaryString(emptyFile);
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandlerForArrayBuffer("", 0, "empty array buffer reading");
  r.readAsArrayBuffer(emptyFile);
  expectedTestCount++;

  r = new FileReader();
  r.onload = getLoadHandler(convertToDataURL(""), 0, "empt binary string reading");
  r.readAsDataURL(emptyFile);
  expectedTestCount++;

  // Test reusing a FileReader to read multiple times
  r = new FileReader();
  r.onload = getLoadHandler(testASCIIData,
                            testASCIIData.length,
                            "to-be-reused reading text")
  var makeAnotherReadListener = function(event) {
    r = event.target;
    r.removeEventListener("load", makeAnotherReadListener, false);
    r.onload = getLoadHandler(testASCIIData,
                              testASCIIData.length,
                              "reused reading text");
    r.readAsText(asciiFile);
  };
  r.addEventListener("load", makeAnotherReadListener, false);
  r.readAsText(asciiFile);
  expectedTestCount += 2;

  r = new FileReader();
  r.onload = getLoadHandler(testBinaryData,
                            testBinaryData.length,
                            "to-be-reused reading binary")
  var makeAnotherReadListener2 = function(event) {
    r = event.target;
    r.removeEventListener("load", makeAnotherReadListener2, false);
    r.onload = getLoadHandler(testBinaryData,
                              testBinaryData.length,
                              "reused reading binary");
    r.readAsBinaryString(binaryFile);
  };
  r.addEventListener("load", makeAnotherReadListener2, false);
  r.readAsBinaryString(binaryFile);
  expectedTestCount += 2;

  r = new FileReader();
  r.onload = getLoadHandler(convertToDataURL(testBinaryData),
                            testBinaryData.length,
                            "to-be-reused reading data url")
  var makeAnotherReadListener3 = function(event) {
    r = event.target;
    r.removeEventListener("load", makeAnotherReadListener3, false);
    r.onload = getLoadHandler(convertToDataURL(testBinaryData),
                              testBinaryData.length,
                              "reused reading data url");
    r.readAsDataURL(binaryFile);
  };
  r.addEventListener("load", makeAnotherReadListener3, false);
  r.readAsDataURL(binaryFile);
  expectedTestCount += 2;

  r = new FileReader();
  r.onload = getLoadHandlerForArrayBuffer(testBinaryData,
                                          testBinaryData.length,
                                          "to-be-reused reading arrayBuffer")
  var makeAnotherReadListener4 = function(event) {
    r = event.target;
    r.removeEventListener("load", makeAnotherReadListener4, false);
    r.onload = getLoadHandlerForArrayBuffer(testBinaryData,
                                            testBinaryData.length,
                                            "reused reading arrayBuffer");
    r.readAsArrayBuffer(binaryFile);
  };
  r.addEventListener("load", makeAnotherReadListener4, false);
  r.readAsArrayBuffer(binaryFile);
  expectedTestCount += 2;

  // Test first reading as ArrayBuffer then read as something else
  // (BinaryString) and doesn't crash
  r = new FileReader();
  r.onload = getLoadHandlerForArrayBuffer(testBinaryData,
                                          testBinaryData.length,
                                          "to-be-reused reading arrayBuffer")
  var makeAnotherReadListener5 = function(event) {
    r = event.target;
    r.removeEventListener("load", makeAnotherReadListener5, false);
    r.onload = getLoadHandler(testBinaryData,
                              testBinaryData.length,
                              "reused reading binary string");
    r.readAsBinaryString(binaryFile);
  };
  r.addEventListener("load", makeAnotherReadListener5, false);
  r.readAsArrayBuffer(binaryFile);
  expectedTestCount += 2;

  //Test data-URI encoding on differing file sizes
  is(dataurldata0.length % 3, 0, "Want to test data with length % 3 == 0");
  r = new FileReader();
  r.onload = getLoadHandler(convertToDataURL(dataurldata0),
                            dataurldata0.length,
                            "dataurl reading, %3 = 0");
  r.readAsDataURL(dataUrlFile0);
  expectedTestCount++;

  is(dataurldata1.length % 3, 1, "Want to test data with length % 3 == 1");
  r = new FileReader();
  r.onload = getLoadHandler(convertToDataURL(dataurldata1),
                            dataurldata1.length,
                            "dataurl reading, %3 = 1");
  r.readAsDataURL(dataUrlFile1);
  expectedTestCount++;

  is(dataurldata2.length % 3, 2, "Want to test data with length % 3 == 2");
  r = new FileReader();
  r.onload = getLoadHandler(convertToDataURL(dataurldata2),
                            dataurldata2.length,
                            "dataurl reading, %3 = 2");
  r.readAsDataURL(dataUrlFile2),
  expectedTestCount++;


  // Test abort()
  var abortHasRun = false;
  var loadEndHasRun = false;
  r = new FileReader();
  r.onabort = function (event) {
    is(abortHasRun, false, "abort should only fire once");
    is(loadEndHasRun, false, "loadend shouldn't have fired yet");
    abortHasRun = true;
    is(event.target.readyState, FileReader.DONE, "should be DONE while firing onabort");
    is(event.target.error.name, "AbortError", "error set to AbortError for aborted reads");
    is(event.target.result, null, "file data should be null on aborted reads");
  }
  r.onloadend = function (event) {
    is(abortHasRun, true, "abort should fire before loadend");
    is(loadEndHasRun, false, "loadend should only fire once");
    loadEndHasRun = true;
    is(event.target.readyState, FileReader.DONE, "should be DONE while firing onabort");
    is(event.target.error.name, "AbortError", "error set to AbortError for aborted reads");
    is(event.target.result, null, "file data should be null on aborted reads");
  }
  r.onload = function() { ok(false, "load should not fire for aborted reads") };
  r.onerror = function() { ok(false, "error should not fire for aborted reads") };
  r.onprogress = function() { ok(false, "progress should not fire for aborted reads") };
  var abortThrew = false;
  try {
    r.abort();
  } catch(e) {
    abortThrew = true;
  }
  is(abortThrew, true, "abort() must throw if not loading");
  is(abortHasRun, false, "abort() is a no-op unless loading");
  r.readAsText(asciiFile);
  r.abort();
  is(abortHasRun, true, "abort should fire sync");
  is(loadEndHasRun, true, "loadend should fire sync");

  // Test calling readAsX to cause abort()
  var reuseAbortHasRun = false;
  r = new FileReader();
  r.onabort = function (event) {
    is(reuseAbortHasRun, false, "abort should only fire once");
    reuseAbortHasRun = true;
    is(event.target.readyState, FileReader.DONE, "should be DONE while firing onabort");
    is(event.target.error.name, "AbortError", "error set to AbortError for aborted reads");
    is(event.target.result, null, "file data should be null on aborted reads");
  }
  r.onload = function() { ok(false, "load should not fire for aborted reads") };
  var abortThrew = false;
  try {
    r.abort();
  } catch(e) {
    abortThrew = true;
  }
  is(abortThrew, true, "abort() must throw if not loading");
  is(reuseAbortHasRun, false, "abort() is a no-op unless loading");
  r.readAsText(asciiFile);
  r.readAsText(asciiFile);
  is(reuseAbortHasRun, true, "abort should fire sync");
  r.onload = getLoadHandler(testASCIIData, testASCIIData.length, "reuse-as-abort reading");
  expectedTestCount++;


  // Test reading from nonexistent files
  r = new FileReader();
  var didThrow = false;
  r.onerror = function (event) {
    is(event.target.readyState, FileReader.DONE, "should be DONE while firing onerror");
    is(event.target.error.name, "NotFoundError", "error set to NotFoundError for nonexistent files");
    is(event.target.result, null, "file data should be null on aborted reads");
    testHasRun();
  };
  r.onload = function (event) {
    is(false, "nonexistent file shouldn't load! (FIXME: bug 1122788)");
    testHasRun();
  };
  try {
    r.readAsDataURL(nonExistingFile);
    expectedTestCount++;
  } catch(ex) {
    didThrow = true;
  }
  // Once this test passes, we should test that onerror gets called and
  // that the FileReader object is in the right state during that call.
  is(didThrow, false, "shouldn't throw when opening nonexistent file, should fire error instead");


  function getLoadHandler(expectedResult, expectedLength, testName) {
    return function (event) {
      is(event.target.readyState, FileReader.DONE,
         "readyState in test " + testName);
      is(event.target.error, null,
         "no error in test " + testName);
      is(event.target.result, expectedResult,
         "result in test " + testName);
      is(event.lengthComputable, true,
         "lengthComputable in test " + testName);
      is(event.loaded, expectedLength,
         "loaded in test " + testName);
      is(event.total, expectedLength,
         "total in test " + testName);
      testHasRun();
    }
  }

  function getLoadHandlerForArrayBuffer(expectedResult, expectedLength, testName) {
    return function (event) {
      is(event.target.readyState, FileReader.DONE,
         "readyState in test " + testName);
      is(event.target.error, null,
         "no error in test " +  testName);
      is(event.lengthComputable, true,
         "lengthComputable in test " + testName);
      is(event.loaded, expectedLength,
         "loaded in test " + testName);
      is(event.total, expectedLength,
         "total in test " + testName);
      is(event.target.result.byteLength, expectedLength,
         "array buffer size in test " + testName);
      var u8v = new Uint8Array(event.target.result);
      is(String.fromCharCode.apply(String, u8v), expectedResult,
         "array buffer contents in test " + testName);
      u8v = null;
      is(event.target.result.byteLength, expectedLength,
         "array buffer size after gc in test " + testName);
      u8v = new Uint8Array(event.target.result);
      is(String.fromCharCode.apply(String, u8v), expectedResult,
         "array buffer contents after gc in test " + testName);
      testHasRun();
    }
  }

  function testHasRun() {
    //alert(testRanCounter);
    ++testRanCounter;
    if (testRanCounter == expectedTestCount) {
      is(testSetupFinished, true, "test setup should have finished; check for exceptions");
      is(onloadHasRunText, true, "onload text should have fired by now");
      is(onloadHasRunBinary, true, "onload binary should have fired by now");
      finish();
    }
  }

  testSetupFinished = true;
}
