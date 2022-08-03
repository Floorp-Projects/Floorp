/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that NetworkHelper.formatSecurityProtocol returns correct
// protocol version strings.

Object.defineProperty(this, "NetworkHelper", {
  get() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true,
});

const TEST_CASES = [
  {
    description: "TLS_VERSION_1",
    input: 1,
    expected: "TLSv1",
  },
  {
    description: "TLS_VERSION_1.1",
    input: 2,
    expected: "TLSv1.1",
  },
  {
    description: "TLS_VERSION_1.2",
    input: 3,
    expected: "TLSv1.2",
  },
  {
    description: "TLS_VERSION_1.3",
    input: 4,
    expected: "TLSv1.3",
  },
  {
    description: "invalid version",
    input: -1,
    expected: "Unknown",
  },
];

function run_test() {
  info("Testing NetworkHelper.formatSecurityProtocol.");

  for (const { description, input, expected } of TEST_CASES) {
    info("Testing " + description);

    equal(
      NetworkHelper.formatSecurityProtocol(input),
      expected,
      "Got the expected protocol string."
    );
  }
}
