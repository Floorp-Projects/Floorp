// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
// ServiceWorker equivalent of worker_wrapper.js.

var client;
var context;

function ok(a, msg) {
  client.postMessage({type: 'status', status: !!a,
                      msg: a + ": " + msg, context: context});
}

function is(a, b, msg) {
  client.postMessage({type: 'status', status: a === b,
                      msg: a + " === " + b + ": " + msg, context: context });
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

function testDone() {
  client.postMessage({ type: 'finish', context: context });
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
  client.postMessage({
    type: 'getPrefs',
    context: context,
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
  client.postMessage({
    type: 'getPermissions',
    context: context,
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
  client.postMessage({
    context: context,
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
  client.postMessage({
    context: context,
    type: 'getUserAgent'
  });
}

var completeInstall = null;

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;
  function runScript() {
    try {
      importScripts(data.script);
    } catch(e) {
      client.postMessage({
        type: 'status',
        status: false,
        context: context,
        msg: 'worker failed to import ' + data.script + "; error: " + e.message
      });
    }
  }
  if ("ServiceWorker" in self) {
    self.clients.matchAll({ includeUncontrolled: true }).then(function(clients) {
      for (var i = 0; i < clients.length; ++i) {
        if (clients[i].url.indexOf("message_receiver.html") > -1) {
          client = clients[i];
          break;
        }
      }
      if (!client) {
        dump("We couldn't find the message_receiver window, the test will fail\n");
      }
      context = "ServiceWorker";
      runScript();
      completeInstall();
    });
  } else {
    client = self;
    context = "Worker";
    runScript();
  }
});

addEventListener("install", e => {
  e.waitUntil(new Promise(resolve => completeInstall = resolve));
})
