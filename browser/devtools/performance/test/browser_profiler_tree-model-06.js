/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when constructing FrameNodes, if optimization data is available,
 * the FrameNodes have the correct optimization data after iterating over samples.
 */

const { RecordingUtils } = devtools.require("devtools/performance/recording-utils");

let gUniqueStacks = new RecordingUtils.UniqueStacks();

function uniqStr(s) {
  return gUniqueStacks.getOrAddStringIndex(s);
}

let time = 1;

let gThread = RecordingUtils.deflateThread({
  samples: [{
    time: 0,
    frames: [
      { location: "(root)" }
    ]
  }, {
    time: time++,
    frames: [
      { location: "(root)" },
      { location: "A_O1" },
      { location: "B" },
      { location: "C" }
    ]
  }, {
    time: time++,
    frames: [
      { location: "(root)" },
      { location: "A_O1" },
      { location: "D" },
      { location: "C" }
    ]
  }, {
    time: time++,
    frames: [
      { location: "(root)" },
      { location: "A_O2" },
      { location: "E_O3" },
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
  }],
  markers: []
}, gUniqueStacks);

// 3 OptimizationSites
let gRawSite1 = {
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
        keyedBy: uniqStr("constructor"),
        name: uniqStr("Foo"),
        location: uniqStr("A (http://foo/bar/baz:12)")
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

let gRawSite2 = {
  line: 34,
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

let gRawSite3 = {
  line: 78,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
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
      [uniqStr("GenericSuccess"), uniqStr("SomeGetter3")]
    ]
  }
};

gThread.frameTable.data.forEach((frame) => {
  const LOCATION_SLOT = gThread.frameTable.schema.location;
  const OPTIMIZATIONS_SLOT = gThread.frameTable.schema.optimizations;

  let l = gThread.stringTable[frame[LOCATION_SLOT]];
  switch (l) {
  case "A_O1":
    frame[LOCATION_SLOT] = uniqStr("A");
    frame[OPTIMIZATIONS_SLOT] = gRawSite1;
    break;
  case "A_O2":
    frame[LOCATION_SLOT] = uniqStr("A");
    frame[OPTIMIZATIONS_SLOT] = gRawSite2;
    break;
  case "E_O3":
    frame[LOCATION_SLOT] = uniqStr("E");
    frame[OPTIMIZATIONS_SLOT] = gRawSite3;
    break;
  }
});

function test() {
  let { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");

  let root = new ThreadNode(gThread);

  let A = getFrameNodePath(root, "(root) > A");

  let opts = A.getOptimizations();
  let sites = opts.optimizationSites;
  is(sites.length, 2, "Frame A has two optimization sites.");
  is(sites[0].samples, 2, "first opt site has 2 samples.");
  is(sites[1].samples, 1, "second opt site has 1 sample.");

  let E = getFrameNodePath(A, "E");
  opts = E.getOptimizations();
  sites = opts.optimizationSites;
  is(sites.length, 1, "Frame E has one optimization site.");
  is(sites[0].samples, 1, "first opt site has 1 samples.");

  let D = getFrameNodePath(A, "D");
  ok(!D.getOptimizations(),
    "frames that do not have any opts data do not have JITOptimizations instances.");

  finish();
}
