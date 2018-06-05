// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

function serviceWorkerTestExec(testFile) {
  return new Promise(function(resolve, reject) {
    function setupSW(registration) {
      var worker = registration.installing ||
                   registration.waiting ||
                   registration.active;
      var iframe;

      window.addEventListener("message",function onMessage(event) {
        if (event.data.context != "ServiceWorker") {
          return;
        }
        if (event.data.type == 'finish') {
          window.removeEventListener("message", onMessage);
          iframe.remove();
          registration.unregister()
            .then(resolve)
            .catch(reject);
        } else if (event.data.type == 'status') {
          ok(event.data.status, event.data.context + ": " + event.data.msg);
        }
      });

      worker.onerror = reject;

      iframe = document.createElement("iframe");
      iframe.src = "message_receiver.html";
      iframe.onload = function() {
        worker.postMessage({ script: testFile });
      };
      document.body.appendChild(iframe);
    }

    navigator.serviceWorker.register("worker_wrapper.js", {scope: "."})
      .then(setupSW);
  });
}
