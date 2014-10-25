var gData1 = "TEST_DATA_1:ABCDEFGHIJKLMNOPQRSTUVWXYZ";
var gData2 = "TEST_DATA_2:1234567890";
var gPaddingChar = '.';
var gPaddingSize = 10000;
var gPadding = "";

for (var i = 0; i < gPaddingSize; i++) {
  gPadding += gPaddingChar;
}

function ok(a, msg) {
  postMessage({type: 'status', status: !!a, msg: msg });
}

function is(a, b, msg) {
  postMessage({type: 'status', status: a === b, msg: msg });
}

function checkData(response, data_head, cb) {
  ok(response, "Data is non-null");
  var str = String.fromCharCode.apply(null, new Uint8Array(response));
  ok(str.length == data_head.length + gPaddingSize, "Data size is correct");
  ok(str.slice(0, data_head.length) == data_head, "Data head is correct");
  ok(str.slice(data_head.length) == gPadding, "Data padding is correct");
  cb();
}

self.onmessage = function onmessage(event) {

  function test_mapped_sync() {
    var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});
    xhr.open('GET', 'jar:http://example.org/tests/dom/base/test/file_bug945152.jar!/data_1.txt', false);
    xhr.responseType = 'arraybuffer';
    xhr.send();
    if (xhr.status) {
      ok(xhr.status == 200, "Status is 200");
      var ct = xhr.getResponseHeader("Content-Type");
      ok(ct.indexOf("mem-mapped") != -1, "Data is memory-mapped");
      checkData(xhr.response, gData1, runTests);
    }
  }

  function test_mapped_async() {
    var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});
    xhr.open('GET', 'jar:http://example.org/tests/dom/base/test/file_bug945152.jar!/data_1.txt');
    xhr.responseType = 'arraybuffer';
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== xhr.DONE) {
        return;
      }
      if (xhr.status) {
        ok(xhr.status == 200, "Status is 200");
        var ct = xhr.getResponseHeader("Content-Type");
        ok(ct.indexOf("mem-mapped") != -1, "Data is memory-mapped");
        checkData(xhr.response, gData1, runTests);
       }
    }
    xhr.send();
  }

  // Make sure array buffer retrieved from compressed file in package is
  // handled by memory allocation instead of memory mapping.
  function test_non_mapped() {
    var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});
    xhr.open('GET', 'jar:http://example.org/tests/dom/base/test/file_bug945152.jar!/data_2.txt');
    xhr.responseType = 'arraybuffer';
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== xhr.DONE) {
        return;
      }
      if (xhr.status) {
        ok(xhr.status == 200, "Status is 200");
        var ct = xhr.getResponseHeader("Content-Type");
        ok(ct.indexOf("mem-mapped") == -1, "Data is not memory-mapped");
        checkData(xhr.response, gData2, runTests);
       }
    }
    xhr.send();
  }

  var tests = [
    test_mapped_sync,
    test_mapped_async,
    test_non_mapped
  ];

  function runTests() {
    if (!tests.length) {
      postMessage({type: 'finish' });
      return;
    }

    var test = tests.shift();
    test();
  }

  runTests();
};
