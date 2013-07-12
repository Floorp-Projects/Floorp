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
      .then(testProfilerCloseWhenClosed)
  }).then(finishUp);
}

function setupGlobals() {
  let deferred = promise.defer();
  gPanel = gDevTools.getToolbox(gTarget).getPanel("jsprofiler");
  deferred.resolve();
  return deferred.promise;
}

function testProfilerStart() {
  let deferred = promise.defer();

  gPanel.once("started", function () {
    is(gPanel.profiles.size, 1, "There is a new profile");
    is(gPanel.getProfileByName("Profile 1"), gPanel.recordingProfile, "Recording profile is OK");
    ok(!gPanel.activeProfile, "There's no active profile yet");
    cmd("profiler start", gcli.lookup("profilerAlreadyStarted2"));
    deferred.resolve();
  });

  cmd("profiler start", gcli.lookup("profilerStarted2"));
  return deferred.promise;
}

function testProfilerList() {
  cmd("profiler list", /^.*Profile\s1\s\*.*$/);
}

function testProfilerStop() {
  let deferred = promise.defer();

  gPanel.once("stopped", function () {
    is(gPanel.activeProfile, gPanel.getProfileByName("Profile 1"), "Active profile is OK");
    ok(!gPanel.recordingProfile, "There's no recording profile");
    cmd("profiler stop", gcli.lookup("profilerNotStarted3"));
    deferred.resolve();
  });

  cmd("profiler stop");
  return deferred.promise;
}

function testProfilerShow() {
  let deferred = promise.defer();

  gPanel.once("profileSwitched", function () {
    is(gPanel.getProfileByName("Profile 1"), gPanel.activeProfile, "Profile 1 is active");
    cmd('profile show "invalid"', gcli.lookup("profilerNotFound"));
    deferred.resolve();
  });

  cmd('profile show "Profile 1"');
  return deferred.promise;
}

function testProfilerClose() {
  let deferred = promise.defer();

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

function testProfilerCloseWhenClosed() {
  // We need to call this test to make sure there are no
  // errors when executing 'profiler close' on a closed
  // toolbox. See bug 863636 for more info.

  let deferred = promise.defer();

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
