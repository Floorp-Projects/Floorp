/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_fullscreen_display_none() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });

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
    async function(browser) {
      let warning = document.getElementById("fullscreen-warning");
      let warningShownPromise = BrowserTestUtils.waitForAttribute(
        "onscreen",
        warning,
        "true"
      );
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
    }
  );
});

add_task(async function test_fullscreen_pointerlock_conflict() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let fsWarning = document.getElementById("fullscreen-warning");
    let plWarning = document.getElementById("pointerlock-warning");

    is(
      fsWarning.getAttribute("onscreen"),
      null,
      "Should not show full screen warning initially."
    );
    is(
      plWarning.getAttribute("onscreen"),
      null,
      "Should not show pointer lock warning initially."
    );

    let fsWarningShownPromise = BrowserTestUtils.waitForAttribute(
      "onscreen",
      fsWarning,
      "true"
    );

    info("Entering full screen and pointer lock.");
    await SpecialPowers.spawn(browser, [], async () => {
      await content.document.body.requestFullscreen();
      await content.document.body.requestPointerLock();
    });

    await fsWarningShownPromise;
    is(
      fsWarning.getAttribute("onscreen"),
      "true",
      "Should show full screen warning."
    );
    is(
      plWarning.getAttribute("onscreen"),
      null,
      "Should not show pointer lock warning."
    );

    info("Exiting pointerlock");
    await SpecialPowers.spawn(browser, [], async () => {
      await content.document.exitPointerLock();
    });

    is(
      fsWarning.getAttribute("onscreen"),
      "true",
      "Should still show full screen warning."
    );
    is(
      plWarning.getAttribute("onscreen"),
      null,
      "Should not show pointer lock warning."
    );

    // Cleanup
    await document.exitFullscreen();
  });
});
