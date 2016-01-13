// Test that async stacks are limited on recursion.

const defaultAsyncStackLimit = 60;

function recur(n, limit) {
  if (n > 0) {
    return callFunctionWithAsyncStack(function recur() {return recur(n - 1, limit)},
                                      saveStack(limit), "Recurse");
  }
  return saveStack(limit);
}

function checkRecursion(n, limit) {
  print("checkRecursion(" + uneval(n) + ", " + uneval(limit) + ")");

  try {
    var stack = recur(n, limit);
  } catch (e) {
    // Some platforms, like ASAN builds, can end up overrecursing. Tolerate
    // these failures.
    assertEq(/too much recursion/.test("" + e), true);
    return;
  }

  // Async stacks are limited even if we didn't ask for a limit. There is a
  // default limit on frames attached on top of any synchronous frames, and
  // every time the limit is reached when capturing, half of the frames are
  // truncated from the old end of the async stack.
  if (limit == 0) {
    // Always add one synchronous frame that is the last call to `recur`.
    if (n + 1 < defaultAsyncStackLimit) {
      limit = defaultAsyncStackLimit + 1;
    } else {
      limit = n + 2 - (defaultAsyncStackLimit / 2);
    }
  }

  // The first `n` or `limit` frames should have `recur` as their `asyncParent`.
  for (var i = 0; i < Math.min(n, limit); i++) {
    assertEq(stack.functionDisplayName, "recur");
    assertEq(stack.parent, null);
    stack = stack.asyncParent;
  }

  // This frame should be the first call to `recur`.
  if (limit > n) {
    assertEq(stack.functionDisplayName, "recur");
    assertEq(stack.asyncParent, null);
    stack = stack.parent;
  } else {
    assertEq(stack, null);
  }

  // This frame should be the call to `checkRecursion`.
  if (limit > n + 1) {
    assertEq(stack.functionDisplayName, "checkRecursion");
    assertEq(stack.asyncParent, null);
    stack = stack.parent;
  } else {
    assertEq(stack, null);
  }

  // We should be at the top frame, which is the test script itself.
  if (limit > n + 2) {
    assertEq(stack.functionDisplayName, null);
    assertEq(stack.asyncParent, null);
    assertEq(stack.parent, null);
  } else {
    assertEq(stack, null);
  }
}

// Check capturing with no limit, which should still apply a default limit.
checkRecursion(0, 0);
checkRecursion(1, 0);
checkRecursion(2, 0);
checkRecursion(defaultAsyncStackLimit - 10, 0);
checkRecursion(defaultAsyncStackLimit, 0);
checkRecursion(defaultAsyncStackLimit + 10, 0);

// Limit of 1 frame.
checkRecursion(0, 1);
checkRecursion(1, 1);
checkRecursion(2, 1);

// Limit of 2 frames.
checkRecursion(0, 2);
checkRecursion(1, 2);
checkRecursion(2, 2);

// Limit of 3 frames.
checkRecursion(0, 3);
checkRecursion(1, 3);
checkRecursion(2, 3);

// Limit higher than the default limit.
checkRecursion(defaultAsyncStackLimit + 10, defaultAsyncStackLimit + 10);
checkRecursion(defaultAsyncStackLimit + 11, defaultAsyncStackLimit + 10);
checkRecursion(defaultAsyncStackLimit + 12, defaultAsyncStackLimit + 10);
