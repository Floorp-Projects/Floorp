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
  ok(frames.length > 0, "Should have captured some frames.");

  let unexpectedRects = 0;
  let alreadyFocused = false;
  for (let i = 1; i < frames.length; ++i) {
    let frame = frames[i], previousFrame = frames[i - 1];
    let rects = compareFrames(frame, previousFrame);

    // The first screenshot we get in OSX / Windows shows an unfocused browser
    // window for some reason. See bug 1445161.
    //
    // We'll assume the changes we are seeing are due to this focus change if
    // there are at least 5 areas that changed near the top of the screen, but
    // will only ignore this once (hence the alreadyFocused variable).
    if (!alreadyFocused && rects.length > 5 && rects.every(r => r.y2 < 100)) {
      alreadyFocused = true;
      todo(false,
           "bug 1445161 - the window should be focused at first paint, " + rects.toSource());
      continue;
    }

    rects = rects.filter(rect => {
      let width = frame.width;

      let exceptions = [
        /**
         * Nothing here! Please don't add anything new!
         */
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
