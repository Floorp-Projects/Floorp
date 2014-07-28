function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;
  try {
    importScripts(data.script);
  } catch(e) {
    postMessage({
      type: 'status',
      status: false,
      msg: 'worker failed to import ' + data.script + "; error: " + e.message
    });
  }
  postMessage({ type: 'finish' });
});
