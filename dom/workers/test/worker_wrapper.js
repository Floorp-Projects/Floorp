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

function workerTestGetPrefs(prefs, cb) {
  addEventListener('message', function workerTestGetPrefsCB(e) {
    if (e.data.type != 'returnPrefs' ||
        !workerTestArrayEquals(prefs, e.data.prefs)) {
      return;
    }
    removeEventListener('message', workerTestGetPrefsCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getPrefs',
    prefs: prefs
  });
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

function workerTestGetVersion(cb) {
  addEventListener('message', function workerTestGetVersionCB(e) {
    if (e.data.type !== 'returnVersion') {
      return;
    }
    removeEventListener('message', workerTestGetVersionCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getVersion'
  });
}

function workerTestGetUserAgent(cb) {
  addEventListener('message', function workerTestGetUserAgentCB(e) {
    if (e.data.type !== 'returnUserAgent') {
      return;
    }
    removeEventListener('message', workerTestGetUserAgentCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getUserAgent'
  });
}

function workerTestGetOSCPU(cb) {
  addEventListener('message', function workerTestGetOSCPUCB(e) {
    if (e.data.type !== 'returnOSCPU') {
      return;
    }
    removeEventListener('message', workerTestGetOSCPUCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getOSCPU'
  });
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
});
