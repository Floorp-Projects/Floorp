// Create two different globals whose compartments have two different
// principals. Test getting the first frame on the stack with some given
// principals in various configurations of JS stack and of wanting self-hosted
// frames or not.

const g1 = newGlobal({
  principal: 0xffff
});

const g2 = newGlobal({
  principal: 0xff
});

// Introduce everyone to themselves and each other.
g1.g2 = g2.g2 = g2;
g1.g1 = g2.g1 = g1;

g1.g2obj = g2.eval("new Object");

g1.evaluate(`
  const global = this;

  // Capture the stack back to the first frame in the g2 global.
  function capture(shouldIgnoreSelfHosted = true) {
    return captureFirstSubsumedFrame(global.g2obj, shouldIgnoreSelfHosted);
  }
`, {
  fileName: "script1.js"
});

g2.evaluate(`
  const capture = g1.capture;

  // Use our Function.prototype.bind, not capture.bind (which is ===
  // g1.Function.prototype.bind) so that the generated bound function is in our
  // compartment and has our principals.
  const boundTrue = Function.prototype.bind.call(capture, null, true);
  const boundFalse = Function.prototype.bind.call(capture, null, false);

  function getOldestFrame(stack) {
    while (stack.parent) {
      stack = stack.parent;
    }
    return stack;
  }

  function dumpStack(name, stack) {
    print("Stack " + name + " =");
    while (stack) {
      print("    " + stack.functionDisplayName + " @ " + stack.source);
      stack = stack.parent;
    }
    print();
  }

  // When the youngest frame is not self-hosted, it doesn't matter whether or not
  // we specify that we should ignore self hosted frames when capturing the first
  // frame with the given principals.
  //
  // Stack: iife1 (g2) <- capture (g1)

  (function iife1() {
    const captureTrueStack = capture(true);
    dumpStack("captureTrueStack", captureTrueStack);
    assertEq(getOldestFrame(captureTrueStack).functionDisplayName, "iife1");
    assertEq(getOldestFrame(captureTrueStack).source, "script2.js");

    const captureFalseStack = capture(false);
    dumpStack("captureFalseStack", captureFalseStack);
    assertEq(getOldestFrame(captureFalseStack).functionDisplayName, "iife1");
    assertEq(getOldestFrame(captureFalseStack).source, "script2.js");
  }());

  // When the youngest frame is a self hosted frame, we get two different
  // captured stacks depending on whether or not we ignore self-hosted frames.
  //
  // Stack: iife2 (g2) <- bound function (g2) <- capture (g1)

  (function iife2() {
    const boundTrueStack = boundTrue();
    dumpStack("boundTrueStack", boundTrueStack);
    assertEq(getOldestFrame(boundTrueStack).functionDisplayName, "iife2");
    assertEq(getOldestFrame(boundTrueStack).source, "script2.js");

    const boundFalseStack = boundFalse();
    dumpStack("boundFalseStack", boundFalseStack);
    assertEq(getOldestFrame(boundFalseStack).functionDisplayName !== "iife2", true);
    assertEq(getOldestFrame(boundFalseStack).source, "self-hosted");
  }());
`, {
  fileName: "script2.js"
});
