var mySharedWorker = new SharedWorker("file_spawn_shared_worker.js");
mySharedWorker.port.onmessage = function(ev) {
  parent.postMessage(
    {
      result: "shared-worker-allowed",
      href: document.location.href,
    },
    "*"
  );
  mySharedWorker.port.close();
};
mySharedWorker.onerror = function(evt) {
  evt.preventDefault();
  parent.postMessage(
    {
      result: "shared-worker-blocked",
      href: document.location.href,
    },
    "*"
  );
  mySharedWorker.port.close();
};
mySharedWorker.port.start();
mySharedWorker.port.postMessage("foo");

// --------------------------------------------

let myWorker = new Worker("file_spawn_worker.js");
myWorker.onmessage = function(event) {
  parent.postMessage(
    {
      result: "worker-allowed",
      href: document.location.href,
    },
    "*"
  );
};
myWorker.onerror = function(event) {
  parent.postMessage(
    {
      result: "worker-blocked",
      href: document.location.href,
    },
    "*"
  );
};

// --------------------------------------------

navigator.serviceWorker
  .register("file_spawn_service_worker.js")
  .then(function(reg) {
    // registration worked
    reg.unregister().then(function() {
      parent.postMessage(
        {
          result: "service-worker-allowed",
          href: document.location.href,
        },
        "*"
      );
    });
  })
  .catch(function(error) {
    // registration failed
    parent.postMessage(
      {
        result: "service-worker-blocked",
        href: document.location.href,
      },
      "*"
    );
  });
