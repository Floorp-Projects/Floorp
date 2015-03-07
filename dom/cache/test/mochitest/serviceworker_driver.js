// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

function serviceWorkerTestExec(testFile) {
  return new Promise(function(resolve, reject) {
    function setupSW(registration) {
      var worker = registration.waiting ||
                   registration.active;

      window.onmessage = function(event) {
        if (event.data.type == 'finish') {
          window.onmessage = null;
          registration.unregister()
            .then(resolve)
            .catch(reject);
        } else if (event.data.type == 'status') {
          ok(event.data.status, event.data.msg);
        }
      };

      worker.onerror = reject;

      var iframe = document.createElement("iframe");
      iframe.src = "message_receiver.html";
      iframe.onload = function() {
        worker.postMessage({ script: testFile });
      };
      document.body.appendChild(iframe);
    }

    navigator.serviceWorker.register("worker_wrapper.js", {scope: "."})
      .then(function(registration) {
        if (registration.installing) {
          registration.installing.onstatechange = function(e) {
            e.target.onstatechange = null;
            setupSW(registration);
          };
        } else {
          setupSW(registration);
        }
      });
  });
}
