var state = "from_scope";
var resolvePromiseCallback;

self.onfetch = function(event) {
  if (event.request.url.includes("lifetime_frame.html")) {
    event.respondWith(new Response("iframe_lifetime"));
    return;
  }

  var currentState = state;
  event.waitUntil(
    self.clients.matchAll().then(clients => {
      clients.forEach(client => {
        client.postMessage({ type: "fetch", state: currentState });
      });
    })
  );

  if (event.request.url.includes("update")) {
    state = "update";
  } else if (event.request.url.includes("wait")) {
    event.respondWith(
      new Promise(function(res, rej) {
        if (resolvePromiseCallback) {
          dump("ERROR: service worker was already waiting on a promise.\n");
        }
        resolvePromiseCallback = function() {
          res(new Response("resolve_respondWithPromise"));
        };
      })
    );
    state = "wait";
  } else if (event.request.url.includes("release")) {
    state = "release";
    resolvePromise();
  }
};

function resolvePromise() {
  if (resolvePromiseCallback === undefined || resolvePromiseCallback == null) {
    dump("ERROR: wait promise was not set.\n");
    return;
  }
  resolvePromiseCallback();
  resolvePromiseCallback = null;
}

self.onmessage = function(event) {
  var lastState = state;
  state = event.data;
  if (state === "wait") {
    event.waitUntil(
      new Promise(function(res, rej) {
        if (resolvePromiseCallback) {
          dump("ERROR: service worker was already waiting on a promise.\n");
        }
        resolvePromiseCallback = res;
      })
    );
  } else if (state === "release") {
    resolvePromise();
  }
  event.source.postMessage({ type: "message", state: lastState });
};

self.onpush = function(event) {
  var pushResolve;
  event.waitUntil(
    new Promise(function(resolve) {
      pushResolve = resolve;
    })
  );

  // FIXME(catalinb): push message carry no data. So we assume the only
  // push message we get is "wait"
  self.clients.matchAll().then(function(client) {
    if (!client.length) {
      dump("ERROR: no clients to send the response to.\n");
    }

    client[0].postMessage({ type: "push", state });

    state = "wait";
    if (resolvePromiseCallback) {
      dump("ERROR: service worker was already waiting on a promise.\n");
    } else {
      resolvePromiseCallback = pushResolve;
    }
  });
};
