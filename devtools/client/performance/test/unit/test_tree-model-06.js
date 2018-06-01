/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that when constructing FrameNodes, if optimization data is available,
 * the FrameNodes have the correct optimization data after iterating over samples,
 * and only youngest frames capture optimization data.
 */

add_task(function test() {
  const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  const root = getFrameNodePath(new ThreadNode(gThread, { startTime: 0,
                                                          endTime: 30 }), "(root)");

  const A = getFrameNodePath(root, "A");
  const B = getFrameNodePath(A, "B");
  const C = getFrameNodePath(B, "C");
  const Aopts = A.getOptimizations();
  const Bopts = B.getOptimizations();
  const Copts = C.getOptimizations();

  ok(!Aopts, "A() was never youngest frame, so should not have optimization data");

  equal(Bopts.length, 2, "B() only has optimization data when it was a youngest frame");

  // Check a few properties on the OptimizationSites.
  const optSitesObserved = new Set();
  for (const opt of Bopts) {
    if (opt.data.line === 12) {
      equal(opt.samples, 2, "Correct amount of samples for B()'s first opt site");
      equal(opt.data.attempts.length, 3, "First opt site has 3 attempts");
      equal(opt.data.attempts[0].strategy, "SomeGetter1", "inflated strategy name");
      equal(opt.data.attempts[0].outcome, "Failure1", "inflated outcome name");
      equal(opt.data.types[0].typeset[0].keyedBy, "constructor", "inflates type info");
      optSitesObserved.add("first");
    } else {
      equal(opt.samples, 1, "Correct amount of samples for B()'s second opt site");
      optSitesObserved.add("second");
    }
  }

  ok(optSitesObserved.has("first"), "first opt site for B() was checked");
  ok(optSitesObserved.has("second"), "second opt site for B() was checked");

  equal(Copts.length, 1, "C() always youngest frame, so has optimization data");
});

var gUniqueStacks = new RecordingUtils.UniqueStacks();

function uniqStr(s) {
  return gUniqueStacks.getOrAddStringIndex(s);
}

var gThread = RecordingUtils.deflateThread({
  samples: [{
    time: 0,
    frames: [
      { location: "(root)" }
    ]
  }, {
    time: 10,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B_LEAF_1" }
    ]
  }, {
    time: 15,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B_NOTLEAF" },
      { location: "C" },
    ]
  }, {
    time: 20,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B_LEAF_2" }
    ]
  }, {
    time: 25,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B_LEAF_2" }
    ]
  }],
  markers: []
}, gUniqueStacks);

var gRawSite1 = {
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("B (http://foo/bar:10)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("B (http://foo/bar:10)")
    }, {
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

var gRawSite2 = {
  line: 22,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Failure3"), uniqStr("SomeGetter3")]
    ]
  }
};

function serialize(x) {
  return JSON.parse(JSON.stringify(x));
}

gThread.frameTable.data.forEach((frame) => {
  const LOCATION_SLOT = gThread.frameTable.schema.location;
  const OPTIMIZATIONS_SLOT = gThread.frameTable.schema.optimizations;

  const l = gThread.stringTable[frame[LOCATION_SLOT]];
  switch (l) {
    case "A":
      frame[OPTIMIZATIONS_SLOT] = serialize(gRawSite1);
      break;
  // Rename some of the location sites so we can register different
  // frames with different opt sites
    case "B_LEAF_1":
      frame[OPTIMIZATIONS_SLOT] = serialize(gRawSite2);
      frame[LOCATION_SLOT] = uniqStr("B");
      break;
    case "B_LEAF_2":
      frame[OPTIMIZATIONS_SLOT] = serialize(gRawSite1);
      frame[LOCATION_SLOT] = uniqStr("B");
      break;
    case "B_NOTLEAF":
      frame[OPTIMIZATIONS_SLOT] = serialize(gRawSite1);
      frame[LOCATION_SLOT] = uniqStr("B");
      break;
    case "C":
      frame[OPTIMIZATIONS_SLOT] = serialize(gRawSite1);
      break;
  }
});
