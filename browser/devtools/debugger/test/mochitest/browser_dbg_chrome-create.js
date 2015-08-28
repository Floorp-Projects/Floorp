/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a chrome debugger can be created in a new process.
 */

let gProcess;

function test() {
  // Windows XP and 8.1 test slaves are terribly slow at this test.
  requestLongerTimeout(5);
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

  initChromeDebugger(aOnClose).then(aProcess => {
    gProcess = aProcess;

    info("Starting test...");
    performTest();
  });
}

function performTest() {
  ok(gProcess._dbgProcess,
    "The remote debugger process wasn't created properly!");
  ok(gProcess._dbgProcess.isRunning,
    "The remote debugger process isn't running!");
  is(typeof gProcess._dbgProcess.pid, "number",
    "The remote debugger process doesn't have a pid (?!)");

  info("process location: " + gProcess._dbgProcess.location);
  info("process pid: " + gProcess._dbgProcess.pid);
  info("process name: " + gProcess._dbgProcess.processName);
  info("process sig: " + gProcess._dbgProcess.processSignature);

  ok(gProcess._dbgProfilePath,
    "The remote debugger profile wasn't created properly!");
  is(gProcess._dbgProfilePath, OS.Path.join(OS.Constants.Path.profileDir, "chrome_debugger_profile"),
     "The remote debugger profile isn't where we expect it!");

  info("profile path: " + gProcess._dbgProfilePath);

  gProcess.close();
}

function aOnClose() {
  ok(!gProcess._dbgProcess.isRunning,
    "The remote debugger process isn't closed as it should be!");
  is(gProcess._dbgProcess.exitValue, (Services.appinfo.OS == "WINNT" ? 0 : 256),
    "The remote debugger process didn't die cleanly.");

  info("process exit value: " + gProcess._dbgProcess.exitValue);

  info("profile path: " + gProcess._dbgProfilePath);

  finish();
}

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  gProcess = null;
});
