"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

function waitFullscreenEvents(aDocument) {
  return new Promise(resolve => {
    function errorHandler() {
      ok(false, "should not get fullscreenerror event");
    }

    let countFsChange = 0;
    function changeHandler() {
      ++countFsChange;
      if (countFsChange == 2) {
        aDocument.removeEventListener("fullscreenchange", changeHandler);
        aDocument.removeEventListener("fullscreenerror", errorHandler);
        resolve();
      }
    }

    aDocument.addEventListener("fullscreenchange", changeHandler);
    aDocument.addEventListener("fullscreenerror", errorHandler);
  });
}

add_task(async function init() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );
});

add_task(async function navigation() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <button id="button">Click here</button>
      <script>
        let button = document.getElementById("button");
        button.addEventListener("click", function() {
          button.requestFullscreen();
          location.href = "about:blank";
        });
      </script>`,
    },
    async function(browser) {
      let promiseFsEvents = waitFullscreenEvents(document);

      await BrowserTestUtils.synthesizeMouseAtCenter("#button", {}, browser);
      await promiseFsEvents;

      ok(!window.fullScreen, "The chrome window should not be in fullscreen");
      ok(
        !document.documentElement.hasAttribute("inDOMFullscreen"),
        "The chrome document should not be in fullscreen"
      );
    }
  );
});
