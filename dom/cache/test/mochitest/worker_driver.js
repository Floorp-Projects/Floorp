// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
// Utility script for writing worker tests.  In your main document do:
//
//  <script type="text/javascript" src="worker_driver.js"></script>
//  <script type="text/javascript">
//    workerTestExec('myWorkerTestCase.js')
//  </script>
//
// This will then spawn a worker, define some utility functions, and then
// execute the code in myWorkerTestCase.js.  You can then use these
// functions in your worker-side test:
//
//  ok() - like the SimpleTest assert
//  is() - like the SimpleTest assert
//  workerTestDone() - like SimpleTest.finish() indicating the test is complete
//
// There are also some functions for requesting information that requires
// SpecialPowers or other main-thread-only resources:
//
//  workerTestGetPrefs() - request an array of prefs value from the main thread
//  workerTestGetPermissions() - request an array permissions from the MT
//  workerTestGetVersion() - request the current version string from the MT
//  workerTestGetUserAgent() - request the user agent string from the MT
//
// For an example see test_worker_interfaces.html and test_worker_interfaces.js.

function workerTestExec(script) {
  return new Promise(function(resolve, reject) {
    var worker = new Worker('worker_wrapper.js');
    worker.onmessage = function(event) {
      is(event.data.context, "Worker",
         "Correct context for messages received on the worker");
      if (event.data.type == 'finish') {
        worker.terminate();
        SpecialPowers.forceGC();
        resolve();

      } else if (event.data.type == 'status') {
        ok(event.data.status, event.data.context + ": " + event.data.msg);

      } else if (event.data.type == 'getPrefs') {
        var result = {};
        event.data.prefs.forEach(function(pref) {
          result[pref] = SpecialPowers.Services.prefs.getBoolPref(pref);
        });
        worker.postMessage({
          type: 'returnPrefs',
          prefs: event.data.prefs,
          result: result
        });

      } else if (event.data.type == 'getPermissions') {
        var result = {};
        event.data.permissions.forEach(function(permission) {
          result[permission] = SpecialPowers.hasPermission(permission, window.document);
        });
        worker.postMessage({
          type: 'returnPermissions',
          permissions: event.data.permissions,
          result: result
        });

      } else if (event.data.type == 'getVersion') {
        var result = SpecialPowers.Cc['@mozilla.org/xre/app-info;1'].getService(SpecialPowers.Ci.nsIXULAppInfo).version;
        worker.postMessage({
          type: 'returnVersion',
          result: result
        });

      } else if (event.data.type == 'getUserAgent') {
        worker.postMessage({
          type: 'returnUserAgent',
          result: navigator.userAgent
        });
      }
    }

    worker.onerror = function(event) {
      reject('Worker had an error: ' + event.data);
    };

    worker.postMessage({ script: script });
  });
}
