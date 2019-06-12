function test_setup() {
  return new Promise(resolve => {
    const minFileSize = 20000;

    // Create strings containing data we'll test with. We'll want long
    // strings to ensure they span multiple buffers while loading
    let testTextData = "asd b\tlah\u1234w\u00a0r";
    while (testTextData.length < minFileSize) {
      testTextData = testTextData + testTextData;
    }

    let testASCIIData = "abcdef 123456\n";
    while (testASCIIData.length < minFileSize) {
      testASCIIData = testASCIIData + testASCIIData;
    }

    let testBinaryData = "";
    for (let i = 0; i < 256; i++) {
      testBinaryData += String.fromCharCode(i);
    }
    while (testBinaryData.length < minFileSize) {
      testBinaryData = testBinaryData + testBinaryData;
    }

    let dataurldata0 = testBinaryData.substr(0, testBinaryData.length -
                                             testBinaryData.length % 3);
    let dataurldata1 = testBinaryData.substr(0, testBinaryData.length - 2 -
                                             testBinaryData.length % 3);
    let dataurldata2 = testBinaryData.substr(0, testBinaryData.length - 1 -
                                             testBinaryData.length % 3);


    //Set up files for testing
    let openerURL = SimpleTest.getTestFileURL("fileapi_chromeScript.js");
    let opener = SpecialPowers.loadChromeScript(openerURL);

    opener.addMessageListener("files.opened", message => {
      let [
        asciiFile,
        binaryFile,
        nonExistingFile,
        utf8TextFile,
        utf16TextFile,
        emptyFile,
        dataUrlFile0,
        dataUrlFile1,
        dataUrlFile2,
      ] = message;

      resolve({ blobs:{ asciiFile, binaryFile, nonExistingFile, utf8TextFile,
                        utf16TextFile, emptyFile, dataUrlFile0, dataUrlFile1,
                        dataUrlFile2 },
                data: { text: testTextData,
                        ascii: testASCIIData,
                        binary: testBinaryData,
                        url0: dataurldata0,
                        url1: dataurldata1,
                        url2: dataurldata2, }});
    });

    opener.sendAsyncMessage("files.open", [
      testASCIIData,
      testBinaryData,
      null,
      convertToUTF8(testTextData),
      convertToUTF16(testTextData),
      "",
      dataurldata0,
      dataurldata1,
      dataurldata2,
    ]);
  });
}

function runBasicTests(data) {
  return test_basic()
    .then(() => {
      return test_readAsText(data.blobs.asciiFile, data.data.ascii);
    })
    .then(() => {
      return test_readAsBinaryString(data.blobs.binaryFile, data.data.binary);
    })
    .then(() => {
      return test_readAsArrayBuffer(data.blobs.binaryFile, data.data.binary);
    });
}

function runEncodingTests(data) {
  return test_readAsTextWithEncoding(data.blobs.asciiFile, data.data.ascii,
                                     data.data.ascii.length, "")
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.asciiFile, data.data.ascii,
                                         data.data.ascii.length, "iso8859-1");
    })
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.utf8TextFile, data.data.text,
                                         convertToUTF8(data.data.text).length,
                                         "utf8");
    })
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.utf16TextFile, data.data.text,
                                         convertToUTF16(data.data.text).length,
                                         "utf-16");
    })
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.emptyFile, "", 0, "");
    })
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.emptyFile, "", 0, "utf8");
    })
    .then(() => {
      return test_readAsTextWithEncoding(data.blobs.emptyFile, "", 0, "utf-16");
    });
}

function runEmptyTests(data) {
  return test_onlyResult()
    .then(() => {
      return test_readAsText(data.blobs.emptyFile, "");
    })
    .then(() => {
      return test_readAsBinaryString(data.blobs.emptyFile, "");
    })
    .then(() => {
      return test_readAsArrayBuffer(data.blobs.emptyFile, "");
    })
    .then(() => {
      return test_readAsDataURL(data.blobs.emptyFile, convertToDataURL(""), 0);
    });
}

function runTwiceTests(data) {
  return test_readAsTextTwice(data.blobs.asciiFile, data.data.ascii)
    .then(() => {
      return test_readAsBinaryStringTwice(data.blobs.binaryFile,
                                          data.data.binary);
    })
    .then(() => {
      return test_readAsDataURLTwice(data.blobs.binaryFile,
                                     convertToDataURL(data.data.binary),
                                     data.data.binary.length);
    })
    .then(() => {
      return test_readAsArrayBufferTwice(data.blobs.binaryFile,
                                         data.data.binary);
    })
    .then(() => {
      return test_readAsArrayBufferTwice2(data.blobs.binaryFile,
                                          data.data.binary);
    });
}

function runOtherTests(data) {
  return test_readAsDataURL_customLength(data.blobs.dataUrlFile0,
                                         convertToDataURL(data.data.url0),
                                         data.data.url0.length, 0)
    .then(() => {
      return test_readAsDataURL_customLength(data.blobs.dataUrlFile1,
                                             convertToDataURL(data.data.url1),
                                             data.data.url1.length, 1);
    })
    .then(() => {
      return test_readAsDataURL_customLength(data.blobs.dataUrlFile2,
                                             convertToDataURL(data.data.url2),
                                             data.data.url2.length, 2);
    })
    .then(() => {
      return test_abort(data.blobs.asciiFile);
    })
    .then(() => {
      return test_abort_readAsX(data.blobs.asciiFile, data.data.ascii);
    })
    .then(() => {
      return test_nonExisting(data.blobs.nonExistingFile);
    });
}

function convertToUTF16(s) {
  let res = "";
  for (let i = 0; i < s.length; ++i) {
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

function loadEventHandler_string(event, resolve, reader, data, dataLength, testName) {
  is(event.target, reader, "Correct target.");
  is(event.target.readyState, FileReader.DONE, "readyState in test " + testName);
  is(event.target.error, null, "no error in test " + testName);
  is(event.target.result, data, "result in test " + testName);
  is(event.lengthComputable, true, "lengthComputable in test " + testName);
  is(event.loaded, dataLength, "loaded in test " + testName);
  is(event.total, dataLength, "total in test " + testName);
  resolve();
}

function loadEventHandler_arrayBuffer(event, resolve, reader, data, testName) {
  is(event.target.readyState, FileReader.DONE, "readyState in test " + testName);
  is(event.target.error, null, "no error in test " +  testName);
  is(event.lengthComputable, true, "lengthComputable in test " + testName);
  is(event.loaded, data.length, "loaded in test " + testName);
  is(event.total, data.length, "total in test " + testName);
  is(event.target.result.byteLength, data.length, "array buffer size in test " + testName);

  let u8v = new Uint8Array(event.target.result);
  is(String.fromCharCode.apply(String, u8v), data,
     "array buffer contents in test " + testName);
  u8v = null;

  if ("SpecialPowers" in self) {
    SpecialPowers.gc();

    is(event.target.result.byteLength, data.length,
       "array buffer size after gc in test " + testName);
    u8v = new Uint8Array(event.target.result);
    is(String.fromCharCode.apply(String, u8v), data,
       "array buffer contents after gc in test " + testName);
  }

  resolve();
}

function test_basic() {
  return new Promise(resolve => {
    is(FileReader.EMPTY, 0, "correct EMPTY value");
    is(FileReader.LOADING, 1, "correct LOADING value");
    is(FileReader.DONE, 2, "correct DONE value");
    resolve();
  });
}

function test_readAsText(blob, text) {
  return new Promise(resolve => {
    let onloadHasRun = false;
    let onloadStartHasRun = false;

    let r = new FileReader();
    is(r.readyState, FileReader.EMPTY, "correct initial text readyState");

    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, text.length, "readAsText");
    }

    r.addEventListener("load", () => { onloadHasRun = true });
    r.addEventListener("loadstart", () => { onloadStartHasRun = true });

    r.readAsText(blob);

    is(r.readyState, FileReader.LOADING, "correct loading text readyState");
    is(onloadHasRun, false, "text loading must be async");
    is(onloadStartHasRun, true, "text loadstart should fire sync");
  });
}

function test_readAsBinaryString(blob, text) {
  return new Promise(resolve => {
    let onloadHasRun = false;
    let onloadStartHasRun = false;

    let r = new FileReader();
    is(r.readyState, FileReader.EMPTY, "correct initial binary readyState");

    r.addEventListener("load", function() { onloadHasRun = true });
    r.addEventListener("loadstart", function() { onloadStartHasRun = true });

    r.readAsBinaryString(blob);

    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, text.length, "readAsBinaryString");
    }

    is(r.readyState, FileReader.LOADING, "correct loading binary readyState");
    is(onloadHasRun, false, "binary loading must be async");
    is(onloadStartHasRun, true, "binary loadstart should fire sync");
  });
}

function test_readAsArrayBuffer(blob, text) {
  return new Promise(resolve => {
    let onloadHasRun = false;
    let onloadStartHasRun = false;

    r = new FileReader();
    is(r.readyState, FileReader.EMPTY, "correct initial arrayBuffer readyState");

    r.addEventListener("load", function() { onloadHasRun = true });
    r.addEventListener("loadstart", function() { onloadStartHasRun = true });

    r.readAsArrayBuffer(blob);

    r.onload = event => {
      loadEventHandler_arrayBuffer(event, resolve, r, text, "readAsArrayBuffer");
    }

    is(r.readyState, FileReader.LOADING, "correct loading arrayBuffer readyState");
    is(onloadHasRun, false, "arrayBuffer loading must be async");
    is(onloadStartHasRun, true, "arrayBuffer loadstart should fire sync");
  });
}

// Test a variety of encodings, and make sure they work properly
function test_readAsTextWithEncoding(blob, text, length, charset) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, length, "readAsText-" + charset);
    }
    r.readAsText(blob, charset);
  });
}

// Test get result without reading
function test_onlyResult() {
  return new Promise(resolve => {
    let r = new FileReader();
    is(r.readyState, FileReader.EMPTY, "readyState in test reader get result without reading");
    is(r.error, null, "no error in test reader get result without reading");
    is(r.result, null, "result in test reader get result without reading");
    resolve();
  });
}

function test_readAsDataURL(blob, text, length) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, length, "readAsDataURL");
    }
    r.readAsDataURL(blob);
  });
}

// Test reusing a FileReader to read multiple times
function test_readAsTextTwice(blob, text) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, () => {}, r, text, text.length, "readAsText-reused-once");
    }

    let anotherListener = event => {
      let r1 = event.target;
      r1.removeEventListener("load", anotherListener);
      r1.onload = evt => {
        loadEventHandler_string(evt, resolve, r1, text, text.length, "readAsText-reused-twice");
      }
      r1.readAsText(blob);
    };

    r.addEventListener("load", anotherListener);
    r.readAsText(blob);
  });
}

// Test reusing a FileReader to read multiple times
function test_readAsBinaryStringTwice(blob, text) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, () => {}, r, text, text.length, "readAsBinaryString-reused-once");
    }

    let anotherListener = event => {
      let r1 = event.target;
      r1.removeEventListener("load", anotherListener);
      r1.onload = evt => {
        loadEventHandler_string(evt, resolve, r1, text, text.length, "readAsBinaryString-reused-twice");
      }
      r1.readAsBinaryString(blob);
    };

    r.addEventListener("load", anotherListener);
    r.readAsBinaryString(blob);
  });
}

function test_readAsDataURLTwice(blob, text, length) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, () => {}, r, text, length, "readAsDataURL-reused-once");
    }

    let anotherListener = event => {
      let r1 = event.target;
      r1.removeEventListener("load", anotherListener);
      r1.onload = evt => {
        loadEventHandler_string(evt, resolve, r1, text, length, "readAsDataURL-reused-twice");
      }
      r1.readAsDataURL(blob);
    };

    r.addEventListener("load", anotherListener);
    r.readAsDataURL(blob);
  });
}

function test_readAsArrayBufferTwice(blob, text) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_arrayBuffer(event, () => {}, r, text, "readAsArrayBuffer-reused-once");
    }

    let anotherListener = event => {
      let r1 = event.target;
      r1.removeEventListener("load", anotherListener);
      r1.onload = evt => {
        loadEventHandler_arrayBuffer(evt, resolve, r1, text, "readAsArrayBuffer-reused-twice");
      }
      r1.readAsArrayBuffer(blob);
    };

    r.addEventListener("load", anotherListener);
    r.readAsArrayBuffer(blob);
  });
}

// Test first reading as ArrayBuffer then read as something else (BinaryString)
// and doesn't crash
function test_readAsArrayBufferTwice2(blob, text) {
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_arrayBuffer(event, () => {}, r, text, "readAsArrayBuffer-reused-once2");
    }

    let anotherListener = event => {
      let r1 = event.target;
      r1.removeEventListener("load", anotherListener);
      r1.onload = evt => {
        loadEventHandler_string(evt, resolve, r1, text, text.length, "readAsArrayBuffer-reused-twice2");
      }
      r1.readAsBinaryString(blob);
    };

    r.addEventListener("load", anotherListener);
    r.readAsArrayBuffer(blob);
  });
}

function test_readAsDataURL_customLength(blob, text, length, numb) {
  return new Promise(resolve => {
    is(length % 3, numb, "Want to test data with length %3 == " + numb);
    let r = new FileReader();
    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, length, "dataurl reading, %3 = " + numb);
    }
    r.readAsDataURL(blob);
  });
}

// Test abort()
function test_abort(blob) {
  return new Promise(resolve => {
    let abortHasRun = false;
    let loadEndHasRun = false;

    let r = new FileReader();

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

    let abortThrew = false;
    try {
      r.abort();
    } catch(e) {
      abortThrew = true;
    }

    is(abortThrew, false, "abort() doesn't throw");
    is(abortHasRun, false, "abort() is a no-op unless loading");

    r.readAsText(blob);
    r.abort();

    is(abortHasRun, true, "abort should fire sync");
    is(loadEndHasRun, true, "loadend should fire sync");

    resolve();
  });
}

// Test calling readAsX to cause abort()
function test_abort_readAsX(blob, text) {
  return new Promise(resolve => {
    let reuseAbortHasRun = false;

    let r = new FileReader();
    r.onabort = function (event) {
      is(reuseAbortHasRun, false, "abort should only fire once");
      reuseAbortHasRun = true;
      is(event.target.readyState, FileReader.DONE, "should be DONE while firing onabort");
      is(event.target.error.name, "AbortError", "error set to AbortError for aborted reads");
      is(event.target.result, null, "file data should be null on aborted reads");
    }
    r.onload = function() { ok(false, "load should fire for nested reads"); };

    let abortThrew = false;
    try {
      r.abort();
    } catch(e) {
      abortThrew = true;
    }

    is(abortThrew, false, "abort() should not throw");
    is(reuseAbortHasRun, false, "abort() is a no-op unless loading");
    r.readAsText(blob);

    let readThrew = false;
    try {
      r.readAsText(blob);
    } catch(e) {
      readThrew = true;
    }

    is(readThrew, true, "readAsText() must throw if loading");
    is(reuseAbortHasRun, false, "abort should not fire");

    r.onload = event => {
      loadEventHandler_string(event, resolve, r, text, text.length, "reuse-as-abort reading");
    }
  });
}

// Test reading from nonexistent files
function test_nonExisting(blob) {
  return new Promise(resolve => {
    let r = new FileReader();

    r.onerror = function (event) {
      is(event.target.readyState, FileReader.DONE, "should be DONE while firing onerror");
      is(event.target.error.name, "NotFoundError", "error set to NotFoundError for nonexistent files");
      is(event.target.result, null, "file data should be null on aborted reads");
      resolve();
    };
    r.onload = function (event) {
      is(false, "nonexistent file shouldn't load! (FIXME: bug 1122788)");
    };

    let didThrow = false;
    try {
      r.readAsDataURL(blob);
    } catch(ex) {
      didThrow = true;
    }

    // Once this test passes, we should test that onerror gets called and
    // that the FileReader object is in the right state during that call.
    is(didThrow, false, "shouldn't throw when opening nonexistent file, should fire error instead");
  });
}
