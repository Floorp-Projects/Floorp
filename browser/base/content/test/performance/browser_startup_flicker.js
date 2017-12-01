/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures that there is no unexpected flicker
 * on the first window opened during startup.
 */

add_task(async function() {
  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject;
  await startupRecorder.done;

  // Ensure all the frame data is in the test compartment to avoid traversing
  // a cross compartment wrapper for each pixel.
  let frames = Cu.cloneInto(startupRecorder.data.frames, {});

  let unexpectedRects = 0;
  let alreadyFocused = false;
  for (let i = 1; i < frames.length; ++i) {
    let frame = frames[i], previousFrame = frames[i - 1];
    let rects = compareFrames(frame, previousFrame);

    // The first screenshot we get shows an unfocused browser window for some
    // reason. This is likely due to the test harness, so we want to ignore it.
    // We'll assume the changes we are seeing are due to this focus change if
    // there are at least 5 areas that changed near the top of the screen, but
    // will only ignore this once (hence the alreadyFocused variable).
    if (!alreadyFocused && rects.length > 5 && rects.every(r => r.y2 < 100)) {
      alreadyFocused = true;
      // This is likely an issue caused by the test harness, but log it anyway.
      todo(false,
           "the window should be focused at first paint, " + rects.toSource());
      continue;
    }

    rects = rects.filter(rect => {
      let inRange = (val, min, max) => min <= val && val <= max;
      let width = frame.width;

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

        {name: "bug 1403648 - urlbar should be focused at first paint",
         condition: r => inRange(r.y2, 60, 80) && // in the toolbar
                         // taking 50% to 75% of the window width
                         inRange(r.w, width * .5, width * .75) &&
                         // starting at 15 to 25% of the window width
                         inRange(r.x1, width * .15, width * .25)
        },

        {name: "bug 1421460 - restore icon should be visible at first paint",
         condition: r => r.w == 9 && r.h == 9 && // 9x9 icon
                         AppConstants.platform == "win" &&
                         // near the right end of the screen
                         inRange(r.x1, width - 80, width - 70)
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
});
