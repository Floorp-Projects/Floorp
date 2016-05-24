var state = "from_scope";
var resolvePromiseCallback;

onfetch = function(event) {
  if (event.request.url.indexOf("lifetime_frame.html") >= 0) {
    event.respondWith(new Response("iframe_lifetime"));
    return;
  }

  var currentState = state;
  event.waitUntil(
    clients.matchAll()
           .then(clients => {
             clients.forEach(client => {
               client.postMessage({type: "fetch", state: currentState});
             });
           })
  );

  if (event.request.url.indexOf("update") >= 0) {
    state = "update";
  } else if (event.request.url.indexOf("wait") >= 0) {
    event.respondWith(new Promise(function(res, rej) {
      if (resolvePromiseCallback) {
        dump("ERROR: service worker was already waiting on a promise.\n");
      }
      resolvePromiseCallback = function() {
        res(new Response("resolve_respondWithPromise"));
      };
    }));
    state = "wait";
  } else if (event.request.url.indexOf("release") >= 0) {
    state = "release";
    resolvePromise();
  }
}

function resolvePromise() {
  if (resolvePromiseCallback === undefined || resolvePromiseCallback == null) {
    dump("ERROR: wait promise was not set.\n");
    return;
  }
  resolvePromiseCallback();
  resolvePromiseCallback = null;
}

onmessage = function(event) {
  // FIXME(catalinb): we cannot treat these events as extendable
  // yet. Bug 1143717
  event.source.postMessage({type: "message", state: state});
  state = event.data;
  if (event.data === "release") {
    resolvePromise();
  }
}

onpush = function(event) {
  var pushResolve;
  event.waitUntil(new Promise(function(resolve) {
    pushResolve = resolve;
  }));

  // FIXME(catalinb): push message carry no data. So we assume the only
  // push message we get is "wait"
  clients.matchAll().then(function(client) {
    if (client.length == 0) {
      dump("ERROR: no clients to send the response to.\n");
    }

    client[0].postMessage({type: "push", state: state});

    state = "wait";
    if (resolvePromiseCallback) {
      dump("ERROR: service worker was already waiting on a promise.\n");
    } else {
      resolvePromiseCallback = pushResolve;
    }
  });
}
