// Test cases when a SavedFrame or one of its ancestors have a principal that is
// not subsumed by the caller's principal, when async parents are present.

function checkVisibleStack(stackFrame, expectedFrames) {
  // We check toString separately from properties like asyncCause to check that
  // it walks the physical SavedFrame chain consistently with the properties.
  var stringFrames = stackFrame.toString().split("\n");

  while (expectedFrames.length) {
    let expectedFrame = expectedFrames.shift();
    let stringFrame = stringFrames.shift();

    // Check the frame properties.
    assertEq(stackFrame.functionDisplayName, expectedFrame.name);
    assertEq(stackFrame.asyncCause, expectedFrame.asyncCause);

    // Check the stringified version.
    let expectedStart =
      (expectedFrame.asyncCause ? expectedFrame.asyncCause + "*" : "") +
      expectedFrame.name;
    assertEq(stringFrame.replace(/@.*$/, ""), expectedStart);

    // If the next frame has an asyncCause, it should be an asyncParent.
    if (expectedFrames.length && expectedFrames[0].asyncCause) {
      assertEq(stackFrame.parent, null);
      stackFrame = stackFrame.asyncParent;
    } else {
      assertEq(stackFrame.asyncParent, null);
      stackFrame = stackFrame.parent;
    }
  }
}

var low = newGlobal({ principal: 0 });
var high = newGlobal({ principal: 0xfffff });

// Test with synchronous cross-compartment calls.
//
// With arrows representing child-to-parent links, and fat arrows representing
// child-to-async-parent links, create a SavedFrame stack like this:
//
//     low.e -> high.d => high.c => high.b -> low.a
//
// This stack captured in function `e` would be seen in its complete version if
// accessed by `high`'s compartment, while in `low`'s compartment it would look
// like this:
//
//     low.e => low.a
//
// The asyncCause seen on `low.a` above should not leak information about the
// real asyncCause on `high.c` and `high.d`.
//
// The stack captured in function `d` would be seen in its complete version if
// accessed by `high`'s compartment, while in `low`'s compartment it would look
// like this:
//
//     low.a

// We'll move these functions into the right globals below before invoking them.
function a() {
  b();
}
function b() {
  callFunctionWithAsyncStack(c, saveStack(), "BtoC");
}
function c() {
  callFunctionWithAsyncStack(d, saveStack(), "CtoD");
}
function d() {
  let stackD = saveStack();

  print("high.checkVisibleStack(stackD)");
  checkVisibleStack(stackD, [
    { name: "d", asyncCause: null   },
    { name: "c", asyncCause: "CtoD" },
    { name: "b", asyncCause: "BtoC" },
    { name: "a", asyncCause: null   },
  ]);

  let stackE = e(saveStack(0, low));

  print("high.checkVisibleStack(stackE)");
  checkVisibleStack(stackE, [
    { name: "e", asyncCause: null   },
    { name: "d", asyncCause: null   },
    { name: "c", asyncCause: "CtoD" },
    { name: "b", asyncCause: "BtoC" },
    { name: "a", asyncCause: null   },
  ]);
}
function e(stackD) {
  print("low.checkVisibleStack(stackD)");
  checkVisibleStack(stackD, [
    { name: "a", asyncCause: "Async" },
  ]);

  let stackE = saveStack();

  print("low.checkVisibleStack(stackE)");
  checkVisibleStack(stackE, [
    { name: "e", asyncCause: null    },
    { name: "a", asyncCause: "Async" },
  ]);

  return saveStack(0, high);
}

// Test with asynchronous cross-compartment calls and shared frames.
//
// With arrows representing child-to-parent links, and fat arrows representing
// child-to-async-parent links, create a SavedFrame stack like this:
//
//     low.x => high.v => low.u
//     low.y -> high.v => low.u
//     low.z => high.w -> low.u
//
// This stack captured in functions `x`, `y`, and `z` would be seen in its
// complete version if accessed by `high`'s compartment, while in `low`'s
// compartment it would look like this:
//
//     low.x => low.u
//     low.y => low.u
//     low.z => low.u
//
// The stack captured in function `v` would be seen in its complete version if
// accessed by `high`'s compartment, while in `low`'s compartment it would look
// like this:
//
//     low.u

// We'll move these functions into the right globals below before invoking them.
function u() {
  callFunctionWithAsyncStack(v, saveStack(), "UtoV");
  w();
}
function v() {
  let stackV = saveStack();
  print("high.checkVisibleStack(stackV)");
  checkVisibleStack(stackV, [
    { name: "v", asyncCause: null   },
    { name: "u", asyncCause: "UtoV" },
  ]);

  let stack = saveStack(0, low);
  function xToCall() { return x(stack);};

  let stackX = callFunctionWithAsyncStack(xToCall, saveStack(), "VtoX");

  print("high.checkVisibleStack(stackX)");
  checkVisibleStack(stackX, [
    { name: "x", asyncCause: null   },
    { name: "xToCall", asyncCause: null },
    { name: "v", asyncCause: "VtoX" },
    { name: "u", asyncCause: "UtoV" },
  ]);

  let stackY = y();

  print("high.checkVisibleStack(stackY)");
  checkVisibleStack(stackY, [
    { name: "y", asyncCause: null   },
    { name: "v", asyncCause: null   },
    { name: "u", asyncCause: "UtoV" },
  ]);
}
function w() {
  let stackZ = callFunctionWithAsyncStack(z, saveStack(), "WtoZ");

  print("high.checkVisibleStack(stackZ)");
  checkVisibleStack(stackZ, [
    { name: "z", asyncCause: null   },
    { name: "w", asyncCause: "WtoZ" },
    { name: "u", asyncCause: null   },
  ]);
}
function x(stackV) {
  print("low.checkVisibleStack(stackV)");
  checkVisibleStack(stackV, [
    { name: "u", asyncCause: "UtoV" },
  ]);

  let stackX = saveStack();

  print("low.checkVisibleStack(stackX)");
  checkVisibleStack(stackX, [
    { name: "x", asyncCause: null   },
    { name: "u", asyncCause: "UtoV" },
  ]);

  return saveStack(0, high);
}

function y() {
  let stackY = saveStack();

  print("low.checkVisibleStack(stackY)");
  checkVisibleStack(stackY, [
    { name: "y", asyncCause: null   },
    { name: "u", asyncCause: "UtoV" },
  ]);

  return saveStack(0, high);
}
function z() {
  let stackZ = saveStack();

  print("low.checkVisibleStack(stackZ)");
  checkVisibleStack(stackZ, [
    { name: "z", asyncCause: null    },
    { name: "u", asyncCause: "Async" },
  ]);

  return saveStack(0, high);
}

// Split the functions in their respective globals.
low .eval(a.toSource());
high.eval(b.toSource());
high.eval(c.toSource());
high.eval(d.toSource());
low .eval(e.toSource());

low .b = high.b;
high.e = low .e;

low .eval(u.toSource());
high.eval(v.toSource());
high.eval(w.toSource());
low .eval(x.toSource());
low .eval(y.toSource());
low .eval(z.toSource());

low .v = high.v;
low .w = high.w;
high.x = low .x;
high.y = low .y;
high.z = low .z;

low .high = high;
high.low  = low;

low .eval(checkVisibleStack.toSource());
high.eval(checkVisibleStack.toSource());

// Execute the tests.
low.a();
low.u();
