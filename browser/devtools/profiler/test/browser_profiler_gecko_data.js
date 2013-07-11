/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    function done() {
      tearDown(gTab, () => { gPanel = null; gTab = null; });
    }

    Services.prefs.setBoolPref(SHOW_PLATFORM_DATA, false);
    recordProfile()
      .then(toggleGeckoDataOption)
      .then(recordProfile)
      .then(done);
  });
}

function recordProfile() {
  let deferred = promise.defer();
  let record = gPanel.controls.record;

  gPanel.once("started", () => {
    gPanel.once("parsed", () => {
      // We cannot be sure which data is returned by
      // the profiler within a test. Until we get rid
      // of Cleopatra, at least.
      deferred.resolve();
    });

    record.click();
  });

  record.click();
  return deferred.promise;
}

function toggleGeckoDataOption() {
  ok(!gPanel.showPlatformData, "showPlatformData is not set");

  Services.prefs.setBoolPref(SHOW_PLATFORM_DATA, true);

  ok(gPanel.showPlatformData, "showPlatformData is set");
}
