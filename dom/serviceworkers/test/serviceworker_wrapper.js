// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
// ServiceWorker equivalent of worker_wrapper.js.

var client;

function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + ": " + msg + "\n");
  client.postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + ": " + msg + "\n");
  client.postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
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
  client.postMessage({ type: 'finish' });
}

function workerTestGetHelperData(cb) {
  addEventListener('message', function workerTestGetHelperDataCB(e) {
    if (e.data.type !== 'returnHelperData') {
      return;
    }
    removeEventListener('message', workerTestGetHelperDataCB);
    cb(e.data.result);
  });
  client.postMessage({
    type: 'getHelperData'
  });
}

function workerTestGetStorageManager(cb) {
  addEventListener('message', function workerTestGetStorageManagerCB(e) {
    if (e.data.type !== 'returnStorageManager') {
      return;
    }
    removeEventListener('message', workerTestGetStorageManagerCB);
    cb(e.data.result);
  });
  client.postMessage({
    type: 'getStorageManager'
  });
}

var completeInstall;

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;
  self.clients.matchAll({ includeUncontrolled: true }).then(function(clients) {
    for (var i = 0; i < clients.length; ++i) {
      if (clients[i].url.includes("message_receiver.html")) {
        client = clients[i];
        break;
      }
    }
    try {
      importScripts(data.script);
    } catch(ex) {
      client.postMessage({
        type: 'status',
        status: false,
        msg: 'worker failed to import ' + data.script + "; error: " + ex.message
      });
    }
    completeInstall();
  });
});

addEventListener('install', e => {
  e.waitUntil(new Promise(resolve => completeInstall = resolve));
});
