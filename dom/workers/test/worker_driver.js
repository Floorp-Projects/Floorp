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
//  workerTestGetVersion() - request the current version string from the MT
//  workerTestGetUserAgent() - request the user agent string from the MT
//  workerTestGetOSCPU() - request the navigator.oscpu string from the MT
//
// For an example see test_worker_interfaces.html and test_worker_interfaces.js.

function workerTestExec(script) {
  SimpleTest.waitForExplicitFinish();
  var worker = new Worker('worker_wrapper.js');
  worker.onmessage = function(event) {
    if (event.data.type == 'finish') {
      SimpleTest.finish();

    } else if (event.data.type == 'status') {
      ok(event.data.status, event.data.msg);

    } else if (event.data.type == 'getHelperData') {
      const version = SpecialPowers.Cc["@mozilla.org/xre/app-info;1"].getService(SpecialPowers.Ci.nsIXULAppInfo).version;
      const isNightly = version.endsWith("a1");
      const isEarlyBetaOrEarlier = SpecialPowers.EARLY_BETA_OR_EARLIER;
      const isRelease = !version.includes("a");
      const isDesktop = !/Mobile|Tablet/.test(navigator.userAgent);
      const isMac = /Mac OS/.test(navigator.oscpu);
      const isWindows = /Windows/.test(navigator.oscpu);
      const isAndroid = navigator.userAgent.includes("Android");
      const isLinux = /Linux/.test(navigator.oscpu) && !isAndroid;
      const isInsecureContext = !window.isSecureContext;
      const isFennec = isAndroid && SpecialPowers.Cc["@mozilla.org/android/bridge;1"].getService(SpecialPowers.Ci.nsIAndroidBridge).isFennec;

      const result = {
        version, isNightly, isEarlyBetaOrEarlier, isRelease, isDesktop, isMac,
        isWindows, isAndroid, isLinux, isInsecureContext, isFennec
      };

      worker.postMessage({
        type: 'returnHelperData', result
      });
    }
  }

  worker.onerror = function(event) {
    ok(false, 'Worker had an error: ' + event.data);
    SimpleTest.finish();
  };

  worker.postMessage({ script: script });
}
