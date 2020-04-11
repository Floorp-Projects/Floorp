/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  /**
   * Nothing here! Please don't add anything new!
   */
];

// We'll assume the changes we are seeing are due to this focus change if
// there are at least 5 areas that changed near the top of the screen, or if
// the toolbar background is involved on OSX, but will only ignore this once.
function isLikelyFocusChange(rects) {
  if (rects.length > 5 && rects.every(r => r.y2 < 100)) {
    return true;
  }
  if (
    Services.appinfo.OS == "Darwin" &&
    rects.length == 2 &&
    rects.every(r => r.y1 == 0 && r.h == 33)
  ) {
    return true;
  }
  return false;
}

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows or flickering areas when opening new windows.
 */
add_task(async function() {
  // Flushing all caches helps to ensure that we get consistent
  // behaviour when opening a new window, even if windows have been
  // opened in previous tests.
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  Services.obs.notifyObservers(null, "chrome-flush-caches");

  let win = window.openDialog(
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    "chrome,all,dialog=no,remote,suppressanimation",
    "about:home"
  );

  await disableFxaBadge();

  let alreadyFocused = false;
  let inRange = (val, min, max) => min <= val && val <= max;
  let expectations = {
    expectedReflows: EXPECTED_REFLOWS,
    frames: {
      filter(rects, frame, previousFrame) {
        // The first screenshot we get in OSX / Windows shows an unfocused browser
        // window for some reason. See bug 1445161.
        if (!alreadyFocused && isLikelyFocusChange(rects)) {
          alreadyFocused = true;
          todo(
            false,
            "bug 1445161 - the window should be focused at first paint, " +
              rects.toSource()
          );
          return [];
        }

        return rects;
      },
      exceptions: [
        {
          name: "bug 1421463 - reload toolbar icon shouldn't flicker",
          condition: r =>
            inRange(r.h, 13, 14) &&
            inRange(r.w, 14, 16) && // icon size
            inRange(r.y1, 40, 80) && // in the toolbar
            inRange(r.x1, 65, 100), // near the left side of the screen
        },
        {
          name: "bug 1555842 - the urlbar shouldn't flicker",
          condition: r => {
            let inputFieldRect = win.gURLBar.inputField.getBoundingClientRect();

            return (
              (!AppConstants.DEBUG ||
                (AppConstants.platform == "linux" && AppConstants.ASAN)) &&
              r.x1 >= inputFieldRect.left &&
              r.x2 <= inputFieldRect.right &&
              r.y1 >= inputFieldRect.top &&
              r.y2 <= inputFieldRect.bottom
            );
          },
        },
      ],
    },
  };

  await withPerfObserver(
    async function() {
      // Avoid showing the remotecontrol UI.
      await new Promise(resolve => {
        win.addEventListener(
          "DOMContentLoaded",
          () => {
            delete win.Marionette;
            win.Marionette = { running: false };
            resolve();
          },
          { once: true }
        );
      });

      await TestUtils.topicObserved(
        "browser-delayed-startup-finished",
        subject => subject == win
      );

      let promises = [
        BrowserTestUtils.firstBrowserLoaded(win, false),
        BrowserTestUtils.browserStopped(
          win.gBrowser.selectedBrowser,
          "about:home"
        ),
      ];

      await Promise.all(promises);

      await new Promise(resolve => {
        // 10 is an arbitrary value here, it needs to be at least 2 to avoid
        // races with code initializing itself using idle callbacks.
        (function waitForIdle(count = 10) {
          if (!count) {
            resolve();
            return;
          }
          Services.tm.idleDispatchToMainThread(() => {
            waitForIdle(count - 1);
          });
        })();
      });
    },
    expectations,
    win
  );

  await BrowserTestUtils.closeWindow(win);
});
