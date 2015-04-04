importScripts("utils.js");

function ok(a, msg) {
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;

  var done = function() {
    postMessage({ type: 'finish' });
  }

  try {
    importScripts(data.script);
    // runTest() is provided by the test.
    runTest().then(done, done);
  } catch(e) {
    postMessage({
      type: 'status',
      status: false,
      msg: 'worker failed to import ' + data.script + "; error: " + e.message
    });
    done();
  }
});
