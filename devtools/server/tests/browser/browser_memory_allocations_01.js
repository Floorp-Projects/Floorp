/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const target = await addTabTarget("data:text/html;charset=utf-8,test-doc");
  const memory = await target.getFront("memory");

  await memory.attach();

  await memory.startRecordingAllocations();
  ok(true, "Can start recording allocations");

  // Allocate some objects.
  const [line1, line2, line3] = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      // Use eval to ensure allocating the object in the page's compartment
      return content.eval(
        "(" +
          function () {
            let alloc1, alloc2, alloc3;

            /* eslint-disable max-nested-callbacks */
            (function outer() {
              (function middle() {
                (function inner() {
                  alloc1 = {};
                  alloc1.line = Error().lineNumber;
                  alloc2 = [];
                  alloc2.line = Error().lineNumber;
                  // eslint-disable-next-line new-parens
                  alloc3 = new (function () {})();
                  alloc3.line = Error().lineNumber;
                })();
              })();
            })();
            /* eslint-enable max-nested-callbacks */

            return [alloc1.line, alloc2.line, alloc3.line];
          } +
          ")()"
      );
    }
  );

  const response = await memory.getAllocations();

  await memory.stopRecordingAllocations();
  ok(true, "Can stop recording allocations");

  // Filter out allocations by library and test code, and get only the
  // allocations that occurred in our test case above.

  function isTestAllocation(alloc) {
    const frame = response.frames[alloc];
    return (
      frame &&
      frame.functionDisplayName === "inner" &&
      (frame.line === line1 || frame.line === line2 || frame.line === line3)
    );
  }

  const testAllocations = response.allocations.filter(isTestAllocation);
  ok(
    testAllocations.length >= 3,
    "Should find our 3 test allocations (plus some allocations for the error " +
      "objects used to get line numbers)"
  );

  // For each of the test case's allocations, ensure that the parent frame
  // indices are correct. Also test that we did get an allocation at each
  // line we expected (rather than a bunch on the first line and none on the
  // others, etc).

  const expectedLines = new Set([line1, line2, line3]);
  is(expectedLines.size, 3, "We are expecting 3 allocations");

  for (const alloc of testAllocations) {
    const innerFrame = response.frames[alloc];
    ok(innerFrame, "Should get the inner frame");
    is(innerFrame.functionDisplayName, "inner");
    expectedLines.delete(innerFrame.line);

    const middleFrame = response.frames[innerFrame.parent];
    ok(middleFrame, "Should get the middle frame");
    is(middleFrame.functionDisplayName, "middle");

    const outerFrame = response.frames[middleFrame.parent];
    ok(outerFrame, "Should get the outer frame");
    is(outerFrame.functionDisplayName, "outer");

    // Not going to test the rest of the frames because they are Task.jsm
    // and promise frames and it gets gross. Plus, I wouldn't want this test
    // to start failing if they changed their implementations in a way that
    // added or removed stack frames here.
  }

  is(expectedLines.size, 0, "Should have found all the expected lines");

  await memory.detach();

  await target.destroy();
});
