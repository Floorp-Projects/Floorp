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
  var worker = new Worker("worker_wrapper.js");
  worker.onmessage = function(event) {
    if (event.data.type == "finish") {
      SimpleTest.finish();
    } else if (event.data.type == "status") {
      ok(event.data.status, event.data.msg);
    } else if (event.data.type == "getHelperData") {
      const { AppConstants } = SpecialPowers.Cu.import(
        "resource://gre/modules/AppConstants.jsm",
        {}
      );
      const isNightly = AppConstants.NIGHTLY_BUILD;
      const isEarlyBetaOrEarlier = AppConstants.EARLY_BETA_OR_EARLIER;
      const isRelease = AppConstants.RELEASE_OR_BETA;
      const isDesktop = !/Mobile|Tablet/.test(navigator.userAgent);
      const isMac = AppConstants.platform == "macosx";
      const isWindows = AppConstants.platform == "win";
      const isAndroid = AppConstants.platform == "android";
      const isLinux = AppConstants.platform == "linux";
      const isInsecureContext = !window.isSecureContext;
      // Currently, MOZ_APP_NAME is always "fennec" for all mobile builds, so we can't use AppConstants for this
      const isFennec =
        isAndroid &&
        SpecialPowers.Cc["@mozilla.org/android/bridge;1"].getService(
          SpecialPowers.Ci.nsIAndroidBridge
        ).isFennec;
      const isCrossOriginIsolated = window.crossOriginIsolated;

      const result = {
        isNightly,
        isEarlyBetaOrEarlier,
        isRelease,
        isDesktop,
        isMac,
        isWindows,
        isAndroid,
        isLinux,
        isInsecureContext,
        isFennec,
        isCrossOriginIsolated,
      };

      worker.postMessage({
        type: "returnHelperData",
        result,
      });
    }
  };

  worker.onerror = function(event) {
    ok(false, "Worker had an error: " + event.data);
    SimpleTest.finish();
  };

  worker.postMessage({ script });
}
