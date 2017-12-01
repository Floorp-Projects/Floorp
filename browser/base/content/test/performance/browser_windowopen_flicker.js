/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures that there is no unexpected flicker
 * when opening new windows.
 */

add_task(async function() {
  // Flushing all caches helps to ensure that we get consistent
  // behaviour when opening a new window, even if windows have been
  // opened in previous tests.
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  Services.obs.notifyObservers(null, "chrome-flush-skin-caches");
  Services.obs.notifyObservers(null, "chrome-flush-caches");

  let win = window.openDialog("chrome://browser/content/", "_blank",
                              "chrome,all,dialog=no,remote,suppressanimation",
                              "about:home");

  // Avoid showing the remotecontrol UI.
  await new Promise(resolve => {
    win.addEventListener("DOMContentLoaded", () => {
      delete win.Marionette;
      win.Marionette = {running: false};
      resolve();
    }, {once: true});
  });

  let canvas = win.document.createElementNS("http://www.w3.org/1999/xhtml",
                                            "canvas");
  canvas.mozOpaque = true;
  let ctx = canvas.getContext("2d", {alpha: false, willReadFrequently: true});

  let frames = [];

  let afterPaintListener = event => {
    let width, height;
    canvas.width = width = win.innerWidth;
    canvas.height = height = win.innerHeight;
    ctx.drawWindow(win, 0, 0, width, height, "white",
                   ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_VIEW |
                   ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES |
                   ctx.DRAWWINDOW_USE_WIDGET_LAYERS);
    frames.push({data: Cu.cloneInto(ctx.getImageData(0, 0, width, height).data, {}),
                 width, height});
  };
  win.addEventListener("MozAfterPaint", afterPaintListener);

  await TestUtils.topicObserved("browser-delayed-startup-finished",
                                subject => subject == win);

  await BrowserTestUtils.firstBrowserLoaded(win, false);
  await BrowserTestUtils.browserStopped(win.gBrowser.selectedBrowser, "about:home");

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
  win.removeEventListener("MozAfterPaint", afterPaintListener);

  let unexpectedRects = 0;
  let foundTinyPaint = false;
  for (let i = 1; i < frames.length; ++i) {
    let frame = frames[i], previousFrame = frames[i - 1];
    if (!foundTinyPaint &&
        previousFrame.width == 1 && previousFrame.height == 1) {
      foundTinyPaint = true;
      todo(false, "shouldn't first paint a 1x1px window");
      continue;
    }

    let rects = compareFrames(frame, previousFrame).filter(rect => {
      let inRange = (val, min, max) => min <= val && val <= max;
      let width = frame.width;

      const spaceBeforeFirstTab = AppConstants.platform == "macosx" ? 100 : 0;
      let inFirstTab = r =>
        inRange(r.x1, spaceBeforeFirstTab, spaceBeforeFirstTab + 50) && r.y1 < 30;

      let exceptions = [
        {name: "bug 1403648 - urlbar down arrow shouldn't flicker",
         condition: r => r.h == 5 && inRange(r.w, 8, 9) && // 5x9px area
                         inRange(r.y1, 40, 80) && // in the toolbar
                         // at ~80% of the window width
                         inRange(r.x1, width * .75, width * .9)
        },

        {name: "bug 1394914 - sidebar toolbar icon should be visible at first paint",
         condition: r => r.h == 13 && inRange(r.w, 14, 16) && // icon size
                         inRange(r.y1, 40, 80) && // in the toolbar
                         // near the right end of screen
                         inRange(r.x1, width - 100, width - 50)
        },

        {name: "bug 1421463 - reload toolbar icon shouldn't flicker",
         condition: r => r.h == 13 && inRange(r.w, 14, 16) && // icon size
                         inRange(r.y1, 40, 80) && // in the toolbar
                         // near the left side of the screen
                         inRange(r.x1, 65, 100)
        },

        {name: "bug 1401955 - about:home favicon should be visible at first paint",
         condition: r => inFirstTab(r) && r.h == 14 && r.w == 14
        },

        {name: "bug 1401955 - space for about:home favicon should be there at first paint",
         condition: r => inFirstTab(r) && inRange(r.w, 60, 80) && inRange(r.h, 8, 14)
        },
      ];

      let rectText = `${rect.toSource()}, window width: ${width}`;
      for (let e of exceptions) {
        if (e.condition(rect)) {
          todo(false, e.name + ", " + rectText);
          return false;
        }
      }

      ok(false, "unexpected changed rect: " + rectText);
      return true;
    });
    if (!rects.length) {
      info("ignoring identical frame");
      continue;
    }

    // Before dumping a frame with unexpected differences for the first time,
    // ensure at least one previous frame has been logged so that it's possible
    // to see the differences when examining the log.
    if (!unexpectedRects) {
      dumpFrame(previousFrame);
    }
    unexpectedRects += rects.length;
    dumpFrame(frame);
  }
  is(unexpectedRects, 0, "should have 0 unknown flickering areas");

  await BrowserTestUtils.closeWindow(win);
});
