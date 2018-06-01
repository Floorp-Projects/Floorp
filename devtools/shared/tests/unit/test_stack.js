/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test stack.js.

function run_test() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  const {StackFrameCache} = require("devtools/server/actors/utils/stack");

  const cache = new StackFrameCache();
  cache.initFrames();
  const baseFrame = {
    line: 23,
    column: 77,
    source: "nowhere",
    functionDisplayName: "nobody",
    parent: null,
    asyncParent: null,
    asyncCause: null
  };
  cache.addFrame(baseFrame);

  let event = cache.makeEvent();
  Assert.equal(event[0], null);
  Assert.equal(event[1].functionDisplayName, "nobody");
  Assert.equal(event.length, 2);

  cache.addFrame({
    line: 24,
    column: 78,
    source: "nowhere",
    functionDisplayName: "still nobody",
    parent: null,
    asyncParent: baseFrame,
    asyncCause: "async"
  });

  event = cache.makeEvent();
  Assert.equal(event[0].functionDisplayName, "still nobody");
  Assert.equal(event[0].parent, 0);
  Assert.equal(event[0].asyncParent, 1);
  Assert.equal(event.length, 1);
}
