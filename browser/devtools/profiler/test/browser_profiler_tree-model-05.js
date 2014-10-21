/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if an inverted call tree model can be correctly computed from a samples
 * array.
 */

let time = 1;

let samples = [{
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "D" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "E" },
    { location: "C" }
  ],
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "F" }
  ]
}];

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");

  let root = new ThreadNode(samples, undefined, undefined, undefined, true);

  is(Object.keys(root.calls).length, 2,
     "Should get the 2 youngest frames, not the 1 oldest frame");

  let C = root.calls.C;
  ok(C, "Should have C as a child of the root.");

  is(Object.keys(C.calls).length, 3,
     "Should have 3 frames that called C.");
  ok(C.calls.B, "B called C.");
  ok(C.calls.D, "D called C.");
  ok(C.calls.E, "E called C.");

  is(Object.keys(C.calls.B.calls).length, 1);
  ok(C.calls.B.calls.A, "A called B called C");
  is(Object.keys(C.calls.D.calls).length, 1);
  ok(C.calls.D.calls.A, "A called D called C");
  is(Object.keys(C.calls.E.calls).length, 1);
  ok(C.calls.E.calls.A, "A called E called C");

  let F = root.calls.F;
  ok(F, "Should have F as a child of the root.");

  is(Object.keys(F.calls).length, 1);
  ok(F.calls.B, "B called F");

  is(Object.keys(F.calls.B.calls).length, 1);
  ok(F.calls.B.calls.A, "A called B called F");

  finish();
}
