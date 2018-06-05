function testScript(script) {

  // The framework runs the entire test in many different configurations.
  // On slow platforms and builds this can make the tests likely to
  // timeout while they are still running.  Lengthen the timeout to
  // accomodate this.
  SimpleTest.requestLongerTimeout(2);

  // reroute.html should have set this variable if a service worker is present!
  if (!("isSWPresent" in window)) {
    window.isSWPresent = false;
  }

  function setupPrefs() {
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({
        "set": [["dom.serviceWorkers.enabled", true],
                ["dom.serviceWorkers.testing.enabled", true],
                ["dom.serviceWorkers.idle_timeout", 0],
                ["dom.serviceWorkers.exemptFromPerDomainMax", true]]
      }, resolve);
    });
  }

  function workerTest() {
    return new Promise(function(resolve, reject) {
      var worker = new Worker("worker_wrapper.js");
      worker.onmessage = function(event) {
        if (event.data.context != "Worker") {
          return;
        }
        if (event.data.type == 'finish') {
          resolve();
        } else if (event.data.type == 'status') {
          ok(event.data.status, event.data.context + ": " + event.data.msg);
        }
      }
      worker.onerror = function(event) {
        reject("Worker error: " + event.message);
      };

      worker.postMessage({ "script": script });
    });
  }

  function nestedWorkerTest() {
    return new Promise(function(resolve, reject) {
      var worker = new Worker("nested_worker_wrapper.js");
      worker.onmessage = function(event) {
        if (event.data.context != "NestedWorker") {
          return;
        }
        if (event.data.type == 'finish') {
          resolve();
        } else if (event.data.type == 'status') {
          ok(event.data.status, event.data.context + ": " + event.data.msg);
        }
      }
      worker.onerror = function(event) {
        reject("Nested Worker error: " + event.message);
      };

      worker.postMessage({ "script": script });
    });
  }

  function serviceWorkerTest() {
    var isB2G = !navigator.userAgent.includes("Android") &&
                /Mobile|Tablet/.test(navigator.userAgent);
    if (isB2G) {
      // TODO B2G doesn't support running service workers for now due to bug 1137683.
      dump("Skipping running the test in SW until bug 1137683 gets fixed.\n");
      return Promise.resolve();
    }
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
          worker.postMessage({ script: script });
        };
        document.body.appendChild(iframe);
      }

      navigator.serviceWorker.register("worker_wrapper.js", {scope: "."})
        .then(setupSW);
    });
  }

  function windowTest() {
    return new Promise(function(resolve, reject) {
      var scriptEl = document.createElement("script");
      scriptEl.setAttribute("src", script);
      scriptEl.onload = function() {
        runTest().then(resolve, reject);
      };
      document.body.appendChild(scriptEl);
    });
  }

  SimpleTest.waitForExplicitFinish();
  // We have to run the window, worker and service worker tests sequentially
  // since some tests set and compare cookies and running in parallel can lead
  // to conflicting values.
  setupPrefs()
    .then(function() {
      return windowTest();
    })
    .then(function() {
      return workerTest();
    })
    .then(function() {
      // XXX Bug 1281212 - This makes other, unrelated test suites fail, primarily on WinXP.
      let isWin = navigator.platform.indexOf("Win") == 0;
      return isWin ? undefined : nestedWorkerTest();
    })
    .then(function() {
      return serviceWorkerTest();
    })
    .catch(function(e) {
      ok(false, "Some test failed in " + script);
      info(e);
      info(e.message);
      return Promise.resolve();
    })
    .then(function() {
      if (parent && parent.finishTest) {
        parent.finishTest();
      } else {
        SimpleTest.finish();
      }
    });
}

