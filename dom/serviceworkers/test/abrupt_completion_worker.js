function setMessageHandler(response) {
  onmessage = e => {
    e.source.postMessage(response);
  };
}

setMessageHandler("handler-before-throw");

// importScripts will throw when the ServiceWorker is past the "intalling" state.
importScripts(`empty.js?${Date.now()}`);

// When importScripts throws an uncaught exception, these calls should never be
// made and the message handler should remain responding "handler-before-throw".
setMessageHandler("handler-after-throw");

// There needs to be a fetch handler to avoid the no-fetch optimizaiton,
// which will skip starting up this worker.
onfetch = e => e.respondWith(new Response("handler-after-throw"));
