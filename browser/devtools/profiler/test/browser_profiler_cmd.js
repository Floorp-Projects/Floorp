/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gcli = Cu.import("resource://gre/modules/devtools/gcli.jsm", {}).gcli;
let gTarget, gPanel, gOptions;

function cmd(typed, expected="") {
  helpers.audit(gOptions, [{
    setup: typed,
    exec: { output: expected }
  }]);
}

function test() {
  waitForExplicitFinish();

  helpers.addTabWithToolbar(URL, function (options) {
    gOptions = options;
    gTarget = options.target;

    return gDevTools.showToolbox(options.target, "jsprofiler")
      .then(setupGlobals)
      .then(testProfilerStart)
      .then(testProfilerList)
      .then(testProfilerStop)
      .then(testProfilerClose)
  }).then(finishUp);
}

function setupGlobals() {
  let deferred = Promise.defer();
  gPanel = gDevTools.getToolbox(gTarget).getPanel("jsprofiler");
  deferred.resolve();
  return deferred.promise;
}

function testProfilerStart() {
  let deferred = Promise.defer();

  gPanel.once("started", function () {
    is(gPanel.profiles.size, 2, "There are two profiles");
    ok(!gPanel.getProfileByName("Profile 1").isStarted, "Profile 1 wasn't started");
    ok(gPanel.getProfileByName("Profile 2").isStarted, "Profile 2 was started");
    cmd('profiler start "Profile 2"', "This profile has already been started");
    deferred.resolve();
  });

  cmd("profiler start", gcli.lookup("profilerStarting2"));
  return deferred.promise;
}

function testProfilerList() {
  let deferred = Promise.defer();

  cmd("profiler list", /^.*Profile\s1.*Profile\s2\s\*.*$/);
  deferred.resolve();

  return deferred.promise;
}

function testProfilerStop() {
  let deferred = Promise.defer();

  gPanel.once("stopped", function () {
    ok(!gPanel.getProfileByName("Profile 2").isStarted, "Profile 2 was stopped");
    ok(gPanel.getProfileByName("Profile 2").isFinished, "Profile 2 was stopped");
    cmd('profiler stop "Profile 2"', "This profile has already been completed. " +
      "Use 'profile show' command to see its results");
    cmd('profiler stop "Profile 1"', "This profile has not been started yet. " +
      "Use 'profile start' to start profiling");
    cmd('profiler stop "invalid"', "Profile not found")
    deferred.resolve();
  });

  cmd('profiler stop "Profile 2"', gcli.lookup("profilerStopping2"));
  return deferred.promise;
}

function testProfilerShow() {
  let deferred = Promise.defer();

  is(gPanel.getProfileByName("Profile 2").uid, gPanel.activeProfile.uid,
    "Profile 2 is active");

  gPanel.once("profileSwitched", function () {
    is(gPanel.getProfileByName("Profile 1").uid, gPanel.activeProfile.uid,
      "Profile 1 is active");
    cmd('profile show "invalid"', "Profile not found");
    deferred.resolve();
  });

  cmd('profile show "Profile 1"');
  return deferred.promise;
}

function testProfilerClose() {
  let deferred = Promise.defer();

  helpers.audit(gOptions, [{
    setup: "profiler close",
    completed: false,
    exec: { output: "" }
  }]);

  let toolbox = gDevTools.getToolbox(gOptions.target);
  if (!toolbox) {
    ok(true, "Profiler was closed.");
    deferred.resolve();
  } else {
    toolbox.on("destroyed", function () {
      ok(true, "Profiler was closed.");
      deferred.resolve();
    });
  }

  return deferred.promise;
}

function finishUp() {
  gTarget = null;
  gPanel = null;
  gOptions = null;
  finish();
}