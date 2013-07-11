/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab1, gPanel1;
let gTab2, gPanel2;

// Tests that you can run the profiler in multiple tabs at the same
// time and that closing the debugger panel in one tab doesn't lock
// profilers in other tabs.

registerCleanupFunction(function () {
  gTab1 = gTab2 = gPanel1 = gPanel2 = null;
});

function test() {
  waitForExplicitFinish();

  openTwoTabs()
    .then(startTwoProfiles)
    .then(stopFirstProfile)
    .then(stopSecondProfile)
    .then(closeTabs)
    .then(openTwoTabs)
    .then(startTwoProfiles)
    .then(closeFirstPanel)
    .then(stopSecondProfile)
    .then(closeTabs)
    .then(finish);
}

function openTwoTabs() {
  let deferred = promise.defer();

  setUp(URL, (tab, browser, panel) => {
    gTab1 = tab;
    gPanel1 = panel;

    loadTab(URL, (tab, browser) => {
      gTab2 = tab;
      openProfiler(tab, () => {
        let target = TargetFactory.forTab(tab);
        gPanel2 = gDevTools.getToolbox(target).getPanel("jsprofiler");
        deferred.resolve();
      });
    });
  });

  return deferred.promise;
}

function startTwoProfiles() {
  let deferred = promise.defer();
  gPanel1.controller.start("Profile 1", (err) => {
    ok(!err, "Profile in tab 1 started without errors");
    gPanel2.controller.start("Profile 1", (err) => {
      ok(!err, "Profile in tab 2 started without errors");
      gPanel1.controller.isActive((err, isActive) => {
        ok(isActive, "Profiler is active");
        deferred.resolve();
      });
    });
  });

  return deferred.promise;
}

function stopFirstProfile() {
  let deferred = promise.defer();

  gPanel1.controller.stop("Profile 1", (err, data) => {
    ok(!err, "Profile in tab 1 stopped without errors");
    ok(data, "Profile in tab 1 returned some data");
    deferred.resolve();
  });

  return deferred.promise;
}

function stopSecondProfile() {
  let deferred = promise.defer();

  gPanel2.controller.stop("Profile 1", (err, data) => {
    ok(!err, "Profile in tab 2 stopped without errors");
    ok(data, "Profile in tab 2 returned some data");
    deferred.resolve();
  });

  return deferred.promise;
}

function closeTabs() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

function closeFirstPanel() {
  let target = TargetFactory.forTab(gTab1);
  let toolbox = gDevTools.getToolbox(target);
  return toolbox.destroy;
}
