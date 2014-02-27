/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTarget, gPanel, gOptions;

function cmd(typed, expected="", waitforEvent=null) {
  let eventPromise;
  if (waitforEvent == null) {
    eventPromise = promise.resolve();
  }
  else {
    let deferred = promise.defer();
    gPanel.once(waitforEvent, () => { deferred.resolve(); });
    eventPromise = deferred.promise;
  }

  let commandPromise = helpers.audit(gOptions, [{
    setup: typed,
    exec: { output: expected }
  }]);

  return promise.all([ commandPromise, eventPromise ]);
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
      // We need to call this test twice to make sure there are no
      // errors when executing 'profiler close' on a closed
      // toolbox. See bug 863636 for more info.
      .then(testProfilerClose)
      .then(testProfilerClose);
  }).then(finishUp, helpers.handleError);
}

function setupGlobals() {
  let deferred = promise.defer();
  gPanel = gDevTools.getToolbox(gTarget).getPanel("jsprofiler");
  deferred.resolve();
  return deferred.promise;
}

function testProfilerStart() {
  let expected = gcli.lookup("profilerStarted2");
  return cmd("profiler start", expected, "started").then(() => {
    is(gPanel.profiles.size, 1, "There is a new profile");
    is(gPanel.getProfileByName("Profile 1"), gPanel.recordingProfile, "Recording profile is OK");
    ok(!gPanel.activeProfile, "There's no active profile yet");
    return cmd("profiler start", gcli.lookup("profilerAlreadyStarted2"));
  });
}

function testProfilerList() {
  return cmd("profiler list", /^.*Profile\s1\s\*.*$/);
}

function testProfilerStop() {
  return cmd("profiler stop", gcli.lookup("profilerStopped"), "stopped").then(() => {
    is(gPanel.activeProfile, gPanel.getProfileByName("Profile 1"), "Active profile is OK");
    ok(!gPanel.recordingProfile, "There's no recording profile");
    return cmd("profiler stop", gcli.lookup("profilerNotStarted3"));
  });
}

function testProfilerShow() {
  return cmd('profile show "Profile 1"', "", "profileSwitched").then(() => {
    is(gPanel.getProfileByName("Profile 1"), gPanel.activeProfile, "Profile 1 is active");
    return cmd('profile show "invalid"', gcli.lookup("profilerNotFound"));
  });
}

function testProfilerClose() {
  let deferred = promise.defer();

  helpers.audit(gOptions, [{
    setup: "profiler close",
    exec: { output: "" }
  }]).then(function() {
    let toolbox = gDevTools.getToolbox(gOptions.target);
    if (!toolbox) {
      ok(true, "Profiler was closed.");
      deferred.resolve();
    } else {
      toolbox.on("destroyed", () => {
        ok(true, "Profiler was closed.");
        deferred.resolve();
      });
    }
  });

  return deferred.promise;
};

function finishUp() {
  gTarget = null;
  gPanel = null;
  gOptions = null;
  finish();
}
