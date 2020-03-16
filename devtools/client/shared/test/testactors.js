/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("devtools/shared/protocol/Actor");

class TestActor1 extends Actor {
  constructor(conn, tab) {
    super(conn);
    this.tab = tab;

    this.typeName = "testOne";
    this.requestTypes = {
      ping: TestActor1.prototype.onPing,
    };
  }

  grip() {
    return { actor: this.actorID, test: "TestActor1" };
  }

  onPing() {
    return { pong: "pong" };
  }
}

exports.TestActor1 = TestActor1;
