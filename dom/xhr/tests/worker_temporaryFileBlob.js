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
  if (e.data == 'simple') {
    test_simple();
  } else if (e.data == 'abort') {
    test_abort();
  } else if (e.data == 'reuse') {
    test_reuse();
  } else {
    ok(false, 'Something wrong happened');
  }
}
