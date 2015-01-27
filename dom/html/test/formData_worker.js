function ok(a, msg) {
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function todo(a, msg) {
  postMessage({type: 'todo', status: !!a, msg: a + ": " + msg });
}

importScripts("formData_test.js");

onmessage = function() {
  runTest(function() {
    postMessage({type: 'finish'});
  });
}
