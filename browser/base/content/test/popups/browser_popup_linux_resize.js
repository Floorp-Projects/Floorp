/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function createLinuxResizeTests(aFirstValue, aSecondValue, aMsg) {
  for (let prop of ResizeMoveTest.PropInfo.sizeProps) {
    // For e.g 'outerWidth' this will be 'innerWidth'.
    let otherProp = ResizeMoveTest.PropInfo.crossBoundsMapping[prop];
    let first = {};
    first[prop] = aFirstValue;
    let second = {};
    second[otherProp] = aSecondValue;
    new ResizeMoveTest(
      [first, second],
      /* aInstant */ true,
      `${aMsg} ${prop},${otherProp}`
    );
    new ResizeMoveTest(
      [first, second],
      /* aInstant */ false,
      `${aMsg} ${prop},${otherProp}`
    );
  }
}

createLinuxResizeTests(9, 10, "Resize");
createLinuxResizeTests(10, 0, "Resize revert");
createLinuxResizeTests(10, 10, "Resize repeat");

new ResizeMoveTest(
  [
    { outerWidth: 10 },
    { innerHeight: 10 },
    { innerWidth: 20 },
    { outerHeight: 20 },
    { outerWidth: 30 },
  ],
  /* aInstant */ true,
  "Resize sequence",
  /* aWaitForCompletion */ true
);

new ResizeMoveTest(
  [
    { outerWidth: 10 },
    { innerHeight: 10 },
    { innerWidth: 20 },
    { outerHeight: 20 },
    { outerWidth: 30 },
  ],
  /* aInstant */ false,
  "Resize sequence",
  /* aWaitForCompletion */ true
);
