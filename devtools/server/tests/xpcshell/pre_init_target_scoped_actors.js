/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("devtools/shared/protocol/Actor");

class PreInitTargetScopedActor extends Actor {
  constructor(conn) {
    super(conn);

    this.typeName = "preInitTargetScoped";
    this.requestTypes = {
      ping: this.onPing,
    };
  }

  onPing() {
    return { message: "pong" };
  }
}

exports.PreInitTargetScopedActor = PreInitTargetScopedActor;
