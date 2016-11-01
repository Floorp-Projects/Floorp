importScripts('common_temporaryFileBlob.js');

function info(msg) {
  postMessage({type: 'info', msg: msg});
}

function ok(a, msg) {
  postMessage({type: 'check', what: !!a, msg: msg});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function next() {
  postMessage({type: 'finish'});
}

onmessage = function(e) {
  test_basic();
}
