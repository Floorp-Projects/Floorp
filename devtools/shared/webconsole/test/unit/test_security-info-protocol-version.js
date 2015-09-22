/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that NetworkHelper.formatSecurityProtocol returns correct
// protocol version strings.

const { require } = Components.utils.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

const Ci = Components.interfaces;
const TEST_CASES = [
  {
    description: "TLS_VERSION_1",
    input: 1,
    expected: "TLSv1"
  }, {
    description: "TLS_VERSION_1.1",
    input: 2,
    expected: "TLSv1.1"
  }, {
    description: "TLS_VERSION_1.2",
    input: 3,
    expected: "TLSv1.2"
  }, {
    description: "invalid version",
    input: -1,
    expected: "Unknown"
  },
];

function run_test() {
  do_print("Testing NetworkHelper.formatSecurityProtocol.");

  for (let {description, input, expected} of TEST_CASES) {
    do_print("Testing " + description);

    equal(NetworkHelper.formatSecurityProtocol(input), expected,
      "Got the expected protocol string.");
  }
}
