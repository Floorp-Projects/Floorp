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

// https://bugzilla.mozilla.org/show_bug.cgi?id=1821884
add_task(async function test_reshow_fullscreen_notification() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let newWin = await BrowserTestUtils.openNewBrowserWindow();
    let fsWarning = document.getElementById("fullscreen-warning");

    info("Entering full screen and wait for the fullscreen warning to appear.");
    await SimpleTest.promiseFocus(window);
    await Promise.all([
      waitForWarningState(fsWarning, "onscreen"),
      BrowserTestUtils.waitForEvent(fsWarning, "transitionend"),
      SpecialPowers.spawn(browser, [], async () => {
        content.document.body.requestFullscreen();
      }),
    ]);

    info(
      "Switch focus away from the fullscreen window, the fullscreen warning should still hide automatically."
    );
    await Promise.all([
      waitForWarningState(fsWarning, "hidden"),
      SimpleTest.promiseFocus(newWin),
    ]);

    info(
      "Switch focus back to the fullscreen window, the fullscreen warning should show again."
    );
    await Promise.all([
      waitForWarningState(fsWarning, "onscreen"),
      SimpleTest.promiseFocus(window),
    ]);

    info("Wait for fullscreen warning timed out.");
    await waitForWarningState(fsWarning, "hidden");

    info("The fullscreen warning should not show again.");
    await SimpleTest.promiseFocus(newWin);
    await SimpleTest.promiseFocus(window);
    await new Promise(resolve => {
      requestAnimationFrame(() => {
        requestAnimationFrame(resolve);
      });
    });
    checkWarningState(
      fsWarning,
      "hidden",
      "The fullscreen warning should not show."
    );

    info("Close new browser window.");
    await BrowserTestUtils.closeWindow(newWin);

    info("Exit fullscreen.");
    await document.exitFullscreen();
  });
});

add_task(async function test_fullscreen_reappear() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let fsWarning = document.getElementById("fullscreen-warning");

    info("Entering full screen and wait for the fullscreen warning to appear.");
    await Promise.all([
      waitForWarningState(fsWarning, "onscreen"),
      SpecialPowers.spawn(browser, [], async () => {
        content.document.body.requestFullscreen();
      }),
    ]);

    info("Wait for fullscreen warning timed out.");
    await waitForWarningState(fsWarning, "hidden");

    info("Move mouse to the top of screen.");
    await Promise.all([
      waitForWarningState(fsWarning, "ontop"),
      EventUtils.synthesizeMouse(document.documentElement, 100, 0, {
        type: "mousemove",
      }),
    ]);

    info("Wait for fullscreen warning timed out again.");
    await waitForWarningState(fsWarning, "hidden");

    info("Exit fullscreen.");
    await document.exitFullscreen();
  });
});

// https://bugzilla.mozilla.org/show_bug.cgi?id=1847901
add_task(async function test_fullscreen_warning_disabled() {
  // Disable fullscreen warning
  await SpecialPowers.pushPrefEnv({
    set: [["full-screen-api.warning.timeout", 0]],
  });

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let newWin = await BrowserTestUtils.openNewBrowserWindow();
    let fsWarning = document.getElementById("fullscreen-warning");
    let mut = new MutationObserver(mutations => {
      ok(false, `${mutations[0].attributeName} attribute should not change`);
    });
    mut.observe(fsWarning, {
      attributeFilter: ["hidden", "onscreen", "ontop"],
    });

    info("Entering full screen.");
    await SimpleTest.promiseFocus(window);
    await SpecialPowers.spawn(browser, [], async () => {
      return content.document.body.requestFullscreen();
    });
    // Wait a bit to ensure no state change.
    await new Promise(resolve => {
      requestAnimationFrame(() => {
        requestAnimationFrame(resolve);
      });
    });

    info("The fullscreen warning should still not show after switching focus.");
    await SimpleTest.promiseFocus(newWin);
    await SimpleTest.promiseFocus(window);
    // Wait a bit to ensure no state change.
    await new Promise(resolve => {
      requestAnimationFrame(() => {
        requestAnimationFrame(resolve);
      });
    });

    mut.disconnect();

    info("Close new browser window.");
    await BrowserTestUtils.closeWindow(newWin);

    info("Exit fullscreen.");
    await document.exitFullscreen();
  });

  // Revert the setting to avoid affecting subsequent tests.
  await SpecialPowers.popPrefEnv();
});
