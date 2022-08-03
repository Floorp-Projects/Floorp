/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const protocol = require("devtools/shared/protocol");
const { FrontClassWithSpec } = protocol;

const spec = protocol.generateActorSpec({
  typeName: "testSetupInParent",

  methods: {
    callSetupInParent: {
      request: {},
      response: {},
    },
  },
});

exports.TestSetupInParentActor = protocol.ActorClassWithSpec(spec, {
  async callSetupInParent() {
    // eslint-disable-next-line no-restricted-properties
    this.conn.setupInParent({
      module:
        "chrome://mochitests/content/browser/devtools/server/tests/browser/setup-in-parent.js",
      setupParent: "setupParent",
      args: [{ one: true }, 2, "three"],
    });
  },
});

class TestSetupInParentFront extends FrontClassWithSpec(spec) {}
exports.TestSetupInParentFront = TestSetupInParentFront;
