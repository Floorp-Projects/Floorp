/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a chrome debugger can be created in a new process.
 */

let gProcess;

function test() {
  // Windows XP and 8.1 test slaves are terribly slow at this test.
  requestLongerTimeout(5);

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

  ok(gProcess._dbgProfile,
    "The remote debugger profile wasn't created properly!");
  ok(gProcess._dbgProfile.localDir,
    "The remote debugger profile doesn't have a localDir...");
  ok(gProcess._dbgProfile.rootDir,
    "The remote debugger profile doesn't have a rootDir...");
  ok(gProcess._dbgProfile.name,
    "The remote debugger profile doesn't have a name...");

  info("profile localDir: " + gProcess._dbgProfile.localDir.path);
  info("profile rootDir: " + gProcess._dbgProfile.rootDir.path);
  info("profile name: " + gProcess._dbgProfile.name);

  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
    .createInstance(Ci.nsIToolkitProfileService);

  let profile = profileService.getProfileByName(gProcess._dbgProfile.name);

  ok(profile,
    "The remote debugger profile wasn't *actually* created properly!");
  is(profile.localDir.path, gProcess._dbgProfile.localDir.path,
    "The remote debugger profile doesn't have the correct localDir!");
  is(profile.rootDir.path, gProcess._dbgProfile.rootDir.path,
    "The remote debugger profile doesn't have the correct rootDir!");

  gProcess.close();
}

function aOnClose() {
  ok(!gProcess._dbgProcess.isRunning,
    "The remote debugger process isn't closed as it should be!");
  is(gProcess._dbgProcess.exitValue, (Services.appinfo.OS == "WINNT" ? 0 : 256),
    "The remote debugger process didn't die cleanly.");

  info("process exit value: " + gProcess._dbgProcess.exitValue);

  info("profile localDir: " + gProcess._dbgProfile.localDir.path);
  info("profile rootDir: " + gProcess._dbgProfile.rootDir.path);
  info("profile name: " + gProcess._dbgProfile.name);

  finish();
}

registerCleanupFunction(function() {
  gProcess = null;
});
