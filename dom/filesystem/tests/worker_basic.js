importScripts('filesystem_commons.js');

function finish() {
  postMessage({ type: 'finish' });
}

function ok(a, msg) {
  postMessage({ type: 'test', test: !!a, message: msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function isnot(a, b, msg) {
  ok(a != b, msg);
}

var tests = [
  function() { test_basic(directory, next); },
  function() { test_getFilesAndDirectories(directory, true, next); },
  function() { test_getFiles(directory, false, next); },
  function() { test_getFiles(directory, true, next); },
];

function next() {
  if (!tests.length) {
    finish();
    return;
  }

  var test = tests.shift();
  test();
}

var directory;

onmessage = function(e) {
  directory = e.data;
  next();
}
