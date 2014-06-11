/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gJar1 = "jar:http://example.org/tests/content/base/test/file_bug945152.jar!/data_1.txt";
var gJar2 = "jar:http://example.org/tests/content/base/test/file_bug945152.jar!/data_2.txt";
var gJar3 = "jar:http://example.org/tests/content/base/test/file_bug945152.jar!/data_big.txt";
var gPaddingChar = ".";
var gPaddingSize = 10000;
var gPadding = "";
for (var i = 0; i < gPaddingSize; i++) {
  gPadding += gPaddingChar;
}
var gData1 = "TEST_DATA_1:ABCDEFGHIJKLMNOPQRSTUVWXYZ" + gPadding;
var gData2 = "TEST_DATA_2:1234567890" + gPadding;

function ok(a, msg) {
  postMessage({type: "status", status: !!a, msg: msg });
}

function is(a, b, msg) {
  postMessage({type: "status", status: a === b, msg: msg });
}

function checkData(xhr, data, mapped, cb) {
  var ct = xhr.getResponseHeader("Content-Type");
  if (mapped) {
    ok(ct.indexOf("mem-mapped") != -1, "Data is memory-mapped");
  } else {
    ok(ct.indexOf("mem-mapped") == -1, "Data is not memory-mapped");
  }
  ok(xhr.response, "Data is non-null");
  var str = String.fromCharCode.apply(null, Uint8Array(xhr.response));
  ok(str == data, "Data is correct");
  cb();
}

self.onmessage = function onmessage(event) {

  var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});

  function reset_event_hander() {
    xhr.onerror = function(e) {
      ok(false, "Error: " + e.error + "\n");
    };
    xhr.onprogress = null;
    xhr.onreadystatechange = null;
    xhr.onload = null;
    xhr.onloadend = null;
  }

  function test_chunked_arraybuffer() {
    ok(true, "Test chunked arraybuffer");

    var lastIndex = 0;
    xhr.onprogress = function(event) {
      if (xhr.response) {
        var buf = new Uint8Array(xhr.response);
        var allMatched = true;
        // The content of data cycles from 0 to 9 (i.e. 01234567890123......).
        for (var i = 0; i < buf.length; i++) {
          if (String.fromCharCode(buf[i]) != lastIndex % 10) {
            allMatched = false;
            break;
          }
          lastIndex++;
        }
        ok(allMatched, "Data chunk is correct.  Loaded " +
                        event.loaded + "/" + event.total + " bytes.");
      }
    };
    xhr.onload = runTests;
    xhr.open("GET", gJar3, true);
    xhr.responseType = "moz-chunked-arraybuffer";
    xhr.send();
  }

  var readystatechangeCount = 0;
  var loadCount = 0;
  var loadendCount = 0;

  function checkEventCount(cb) {
    ok(readystatechangeCount == 1 && loadCount == 1 && loadendCount == 1,
       "Saw all expected events");
    cb();
  }

  function test_multiple_events() {
    ok(true, "Test multiple events");
    xhr.abort();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == xhr.DONE) {
        readystatechangeCount++;
        checkData(xhr, gData2, false, function() {} );
      }
    };
    xhr.onload = function() {
      loadCount++;
      checkData(xhr, gData2, false, function() {} );
    };
    xhr.onloadend = function() {
      loadendCount++;
      checkData(xhr, gData2, false, function() {} );
    };
    xhr.open("GET", gJar2, false);
    xhr.responseType = "arraybuffer";
    xhr.send();
    checkEventCount(runTests);
  }

  function test_sync_xhr_data1() {
    ok(true, "Test sync XHR with data1");
    xhr.open("GET", gJar1, false);
    xhr.responseType = "arraybuffer";
    xhr.send();
    checkData(xhr, gData1, true, runTests);
  }

  function test_sync_xhr_data2() {
    ok(true, "Test sync XHR with data2");
    xhr.open("GET", gJar2, false);
    xhr.responseType = "arraybuffer";
    xhr.send();
    checkData(xhr, gData2, false, runTests);
  }

  function test_async_xhr_data1() {
    ok(true, "Test async XHR with data1");
    xhr.onload = function() {
      checkData(xhr, gData1, true, runTests);
    };
    xhr.open("GET", gJar1, true);
    xhr.responseType = "arraybuffer";
    xhr.send();
  }

  function test_async_xhr_data2() {
    ok(true, "Test async XHR with data2");
    xhr.onload = function() {
      checkData(xhr, gData2, false, runTests);
    };
    xhr.open("GET", gJar2, true);
    xhr.responseType = "arraybuffer";
    xhr.send();
  }

  var tests = [
    test_chunked_arraybuffer,
    test_multiple_events,
    test_sync_xhr_data1,
    test_sync_xhr_data2,
    test_async_xhr_data1,
    test_async_xhr_data2
  ];

  function runTests() {
    if (!tests.length) {
      postMessage({type: "finish" });
      return;
    }

    reset_event_hander();

    var test = tests.shift();
    test();
  }

  runTests();
};
