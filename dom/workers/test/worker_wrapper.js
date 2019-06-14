// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
// Worker-side wrapper script for the worker_driver.js helper code.  See
// the comments at the top of worker_driver.js for more information.

function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + ": " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + ": " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function workerTestArrayEquals(a, b) {
  if (!Array.isArray(a) || !Array.isArray(b) || a.length != b.length) {
    return false;
  }
  for (var i = 0, n = a.length; i < n; ++i) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}

function workerTestDone() {
  postMessage({ type: 'finish' });
}

function workerTestGetPermissions(permissions, cb) {
  addEventListener('message', function workerTestGetPermissionsCB(e) {
    if (e.data.type != 'returnPermissions' ||
        !workerTestArrayEquals(permissions, e.data.permissions)) {
      return;
    }
    removeEventListener('message', workerTestGetPermissionsCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getPermissions',
    permissions: permissions
  });
}

function workerTestGetHelperData(cb) {
  addEventListener('message', function workerTestGetHelperDataCB(e) {
    if (e.data.type !== 'returnHelperData') {
      return;
    }
    removeEventListener('message', workerTestGetHelperDataCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getHelperData'
  });
}

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;
  try {
    importScripts(data.script);
  } catch(ex) {
    postMessage({
      type: 'status',
      status: false,
      msg: 'worker failed to import ' + data.script + "; error: " + ex.message
    });
  }
});
