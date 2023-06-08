/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function checkWarningState(aWarningElement, aExpectedState, aMsg) {
  ["hidden", "ontop", "onscreen"].forEach(state => {
    is(
      aWarningElement.hasAttribute(state),
      state == aExpectedState,
      `${aMsg} - check ${state} attribute.`
    );
  });
}

async function waitForWarningState(aWarningElement, aExpectedState) {
  await BrowserTestUtils.waitForAttribute(aExpectedState, aWarningElement, "");
  checkWarningState(
    aWarningElement,
    aExpectedState,
    `Wait for ${aExpectedState} state`
  );
}

add_setup(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });
});

add_task(async function test_fullscreen_display_none() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <html>
        <head>
          <meta charset="utf-8"/>
          <title>Fullscreen Test</title>
        </head>
        <body id="body">
        <iframe
         src="https://example.org/browser/browser/base/content/test/fullscreen/fullscreen.html"
         hidden
         allowfullscreen></iframe>
        </body>
      </html>`,
    },
    async function (browser) {
      let warning = document.getElementById("fullscreen-warning");
      checkWarningState(
        warning,
        "hidden",
        "Should not show full screen warning initially"
      );

      let warningShownPromise = waitForWarningState(warning, "onscreen");
      // Enter fullscreen
      await SpecialPowers.spawn(browser, [], async () => {
        let frame = content.document.querySelector("iframe");
        frame.focus();
        await SpecialPowers.spawn(frame, [], () => {
          content.document.getElementById("request").click();
        });
      });
      await warningShownPromise;
      ok(true, "Fullscreen warning shown");
      // Exit fullscreen
      let exitFullscreenPromise = BrowserTestUtils.waitForEvent(
        document,
        "fullscreenchange",
        false,
        () => !document.fullscreenElement
      );
      document.getElementById("fullscreen-exit-button").click();
      await exitFullscreenPromise;

      checkWarningState(
        warning,
        "hidden",
        "Should hide fullscreen warning after exiting fullscreen"
      );
    }
  );
});

add_task(async function test_fullscreen_pointerlock_conflict() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let fsWarning = document.getElementById("fullscreen-warning");
    let plWarning = document.getElementById("pointerlock-warning");

    checkWarningState(
      fsWarning,
      "hidden",
      "Should not show full screen warning initially"
    );
    checkWarningState(
      plWarning,
      "hidden",
      "Should not show pointer lock warning initially"
    );

    let fsWarningShownPromise = waitForWarningState(fsWarning, "onscreen");
    info("Entering full screen and pointer lock.");
    await SpecialPowers.spawn(browser, [], async () => {
      await content.document.body.requestFullscreen();
      await content.document.body.requestPointerLock();
    });

    await fsWarningShownPromise;
    checkWarningState(
      plWarning,
      "hidden",
      "Should not show pointer lock warning"
    );

    info("Exiting pointerlock");
    await SpecialPowers.spawn(browser, [], async () => {
      await content.document.exitPointerLock();
    });

    checkWarningState(
      fsWarning,
      "onscreen",
      "Should still show full screen warning"
    );
    checkWarningState(
      plWarning,
      "hidden",
      "Should not show pointer lock warning"
    );

    // Cleanup
    info("Exiting fullscreen");
    await document.exitFullscreen();
  });
});
