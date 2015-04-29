// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

function serviceWorkerTestExec(testFile) {
  var isB2G = !navigator.userAgent.includes("Android") &&
              /Mobile|Tablet/.test(navigator.userAgent);
  if (isB2G) {
    // TODO B2G doesn't support running service workers for now due to bug 1137683.
    dump("Skipping running the test in SW until bug 1137683 gets fixed.\n");
    return Promise.resolve();
  }
  return new Promise(function(resolve, reject) {
    function setupSW(registration) {
      var worker = registration.waiting ||
                   registration.active;

      window.addEventListener("message",function onMessage(event) {
        if (event.data.context != "ServiceWorker") {
          return;
        }
        if (event.data.type == 'finish') {
          window.removeEventListener("message", onMessage);
          registration.unregister()
            .then(resolve)
            .catch(reject);
        } else if (event.data.type == 'status') {
          ok(event.data.status, event.data.context + ": " + event.data.msg);
        }
      }, false);

      worker.onerror = reject;

      var iframe = document.createElement("iframe");
      iframe.src = "message_receiver.html";
      iframe.onload = function() {
        worker.postMessage({ script: testFile });
      };
      document.body.appendChild(iframe);
    }

    navigator.serviceWorker.ready.then(setupSW);
    navigator.serviceWorker.register("worker_wrapper.js", {scope: "."});
  });
}
