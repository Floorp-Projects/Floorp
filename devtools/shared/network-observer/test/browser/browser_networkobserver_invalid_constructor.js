/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the NetworkObserver constructor validates its arguments.
add_task(async function testInvalidConstructorArguments() {
  Assert.throws(
    () => new NetworkObserver(),
    /Expected "ignoreChannelFunction" to be a function, got undefined/,
    "NetworkObserver constructor should throw if no argument was provided"
  );

  Assert.throws(
    () => new NetworkObserver({}),
    /Expected "ignoreChannelFunction" to be a function, got undefined/,
    "NetworkObserver constructor should throw if ignoreChannelFunction was not provided"
  );

  const invalidValues = [null, true, false, 12, "str", ["arr"], { obj: "obj" }];
  for (const invalidValue of invalidValues) {
    Assert.throws(
      () => new NetworkObserver({ ignoreChannelFunction: invalidValue }),
      /Expected "ignoreChannelFunction" to be a function, got/,
      `NetworkObserver constructor should throw if a(n) ${typeof invalidValue} was provided for ignoreChannelFunction`
    );
  }

  const EMPTY_FN = () => {};
  Assert.throws(
    () => new NetworkObserver({ ignoreChannelFunction: EMPTY_FN }),
    /Expected "onNetworkEvent" to be a function, got undefined/,
    "NetworkObserver constructor should throw if onNetworkEvent was not provided"
  );

  // Now we will pass a function for `ignoreChannelFunction`, and will do the
  // same tests for onNetworkEvent
  for (const invalidValue of invalidValues) {
    Assert.throws(
      () =>
        new NetworkObserver({
          ignoreChannelFunction: EMPTY_FN,
          onNetworkEvent: invalidValue,
        }),
      /Expected "onNetworkEvent" to be a function, got/,
      `NetworkObserver constructor should throw if a(n) ${typeof invalidValue} was provided for onNetworkEvent`
    );
  }
});
