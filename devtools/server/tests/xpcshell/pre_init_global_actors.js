/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("devtools/shared/protocol/Actor");

class PreInitGlobalActor extends Actor {
  constructor(conn) {
    super(conn);

    this.typeName = "preInitGlobal";
    this.requestTypes = {
      ping: this.onPing,
    };
  }

  onPing() {
    return { message: "pong" };
  }
}

exports.PreInitGlobalActor = PreInitGlobalActor;
