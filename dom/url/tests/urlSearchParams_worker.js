/* eslint-env worker */

importScripts("urlSearchParams_commons.js");

function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: "status", status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a === b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: "status", status: a === b, msg: a + " === " + b + ": " + msg });
}

var tests = [
  testSimpleURLSearchParams,
  testCopyURLSearchParams,
  testParserURLSearchParams,
  testURL,
  testEncoding,
  testCTORs,
];

function runTest() {
  if (!tests.length) {
    postMessage({type: "finish" });
    return;
  }

  var test = tests.shift();
  test();
}

onmessage = function() {
  let status = false;
  try {
    if ((URLSearchParams instanceof Object)) {
      status = true;
    }
  } catch (e) {
  }
  ok(status, "URLSearchParams in workers \\o/");

  runTest();
};
