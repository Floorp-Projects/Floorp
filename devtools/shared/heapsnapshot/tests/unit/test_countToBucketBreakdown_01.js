/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that we can turn a breakdown with { by: "count" } leaves into a
// breakdown with { by: "bucket" } leaves.

const COUNT = { by: "count", count: true, bytes: true };
const BUCKET = { by: "bucket" };

const BREAKDOWN = {
  by: "coarseType",
  objects: { by: "objectClass", then: COUNT, other: COUNT },
  strings: COUNT,
  scripts: {
    by: "filename",
    then: { by: "internalType", then: COUNT },
    noFilename: { by: "internalType", then: COUNT },
  },
  other: { by: "internalType", then: COUNT },
};

const EXPECTED = {
  by: "coarseType",
  objects: { by: "objectClass", then: BUCKET, other: BUCKET },
  strings: BUCKET,
  scripts: {
    by: "filename",
    then: { by: "internalType", then: BUCKET },
    noFilename: { by: "internalType", then: BUCKET },
  },
  other: { by: "internalType", then: BUCKET },
};

function run_test() {
  assertCountToBucketBreakdown(BREAKDOWN, EXPECTED);
}
