/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of a "complex" and "realistic" breakdown.

const BREAKDOWN = {
  by: "coarseType",
  objects: {
    by: "allocationStack",
    then: {
      by: "objectClass",
      then: { by: "count", count: false, bytes: true },
      other: { by: "count", count: false, bytes: true }
    },
    noStack: {
      by: "objectClass",
      then: { by: "count", count: false, bytes: true },
      other: { by: "count", count: false, bytes: true }
    }
  },
  strings: {
    by: "internalType",
    then: { by: "count", count: false, bytes: true }
  },
  scripts: {
    by: "internalType",
    then: { by: "count", count: false, bytes: true }
  },
  other: {
    by: "internalType",
    then: { by: "count", count: false, bytes: true }
  },
};

const stack1 = saveStack();
const stack2 = saveStack();
const stack3 = saveStack();

const REPORT1 = {
  objects: new Map([
    [stack1, { Function: { bytes: 1 },
               Object: { bytes: 2 },
               other: { bytes: 0 },
             }],
    [stack2, { Array: { bytes: 3 },
               Date: { bytes: 4 },
               other: { bytes: 0 },
             }],
    ["noStack", { Object: { bytes: 3 }}],
  ]),
  strings: {
    JSAtom: { bytes: 10 },
    JSLinearString: { bytes: 5 },
  },
  scripts: {
    JSScript: { bytes: 1 },
    "js::jit::JitCode": { bytes: 2 },
  },
  other: {
    "mozilla::dom::Thing": { bytes: 1 },
  }
};

const REPORT2 = {
  objects: new Map([
    [stack2, { Array: { bytes: 1 },
               Date: { bytes: 2 },
               other: { bytes: 3 },
             }],
    [stack3, { Function: { bytes: 1 },
               Object: { bytes: 2 },
               other: { bytes: 0 },
             }],
    ["noStack", { Object: { bytes: 3 }}],
  ]),
  strings: {
    JSAtom: { bytes: 5 },
    JSLinearString: { bytes: 10 },
  },
  scripts: {
    JSScript: { bytes: 2 },
    "js::LazyScript": { bytes: 42 },
    "js::jit::JitCode": { bytes: 1 },
  },
  other: {
    "mozilla::dom::OtherThing": { bytes: 1 },
  }
};

const EXPECTED = {
  "objects": new Map([
    [stack1, { Function: { bytes: -1 },
               Object: { bytes: -2 },
               other: { bytes: 0 },
             }],
    [stack2, { Array: { bytes: -2 },
               Date: { bytes: -2 },
               other: { bytes: 3 },
             }],
    [stack3, { Function: { bytes: 1 },
               Object: { bytes: 2 },
               other: { bytes: 0 },
             }],
    ["noStack", { Object: { bytes: 0 }}],
  ]),
  "scripts": {
    "JSScript": {
      "bytes": 1
    },
    "js::jit::JitCode": {
      "bytes": -1
    },
    "js::LazyScript": {
      "bytes": 42
    }
  },
  "strings": {
    "JSAtom": {
      "bytes": -5
    },
    "JSLinearString": {
      "bytes": 5
    }
  },
  "other": {
    "mozilla::dom::Thing": {
      "bytes": -1
    },
    "mozilla::dom::OtherThing": {
      "bytes": 1
    }
  }
};

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
