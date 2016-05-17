/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that NetworkHelper.getReasonsForWeakness returns correct reasons for
// weak requests.

const { require } = Components.utils.import("resource://devtools/shared/Loader.jsm", {});

Object.defineProperty(this, "NetworkHelper", {
  get: function () {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

var Ci = Components.interfaces;
const wpl = Ci.nsIWebProgressListener;
const TEST_CASES = [
  {
    description: "weak cipher",
    input: wpl.STATE_IS_BROKEN | wpl.STATE_USES_WEAK_CRYPTO,
    expected: ["cipher"]
  }, {
    description: "only STATE_IS_BROKEN flag",
    input: wpl.STATE_IS_BROKEN,
    expected: []
  }, {
    description: "only STATE_IS_SECURE flag",
    input: wpl.STATE_IS_SECURE,
    expected: []
  },
];

function run_test() {
  do_print("Testing NetworkHelper.getReasonsForWeakness.");

  for (let {description, input, expected} of TEST_CASES) {
    do_print("Testing " + description);

    deepEqual(NetworkHelper.getReasonsForWeakness(input), expected,
      "Got the expected reasons for weakness.");
  }
}
