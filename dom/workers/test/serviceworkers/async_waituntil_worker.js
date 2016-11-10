var keepAlivePromise;
var resolvePromise;
var result = "Failed";

onactivate = function(event) {
  event.waitUntil(clients.claim());
}

onmessage = function(event) {
  if (event.data === "Start") {
    event.waitUntil(Promise.reject());

    keepAlivePromise = new Promise(function(resolve, reject) {
      resolvePromise = resolve;
    });

    result = "Success";
    event.waitUntil(keepAlivePromise);
    event.source.postMessage("Started");
  } else if (event.data === "Result") {
    event.source.postMessage(result);
    if (resolvePromise !== undefined) {
      resolvePromise();
    }
  }
}

addEventListener('fetch', e => {
  let respondWithPromise = new Promise(function(res, rej) {
    setTimeout(() => {
      res(new Response("ok"));
    }, 0);
  });
  e.respondWith(respondWithPromise);
  // Test that waitUntil can be called in the promise handler of the existing
  // lifetime extension promise.
  respondWithPromise.then(() => {
    e.waitUntil(clients.matchAll().then((cls) => {
      if (cls.length != 1) {
        dump("ERROR: no controlled clients.\n");
      }
      client = cls[0];
      client.postMessage("Done");
    }));
  });
});
